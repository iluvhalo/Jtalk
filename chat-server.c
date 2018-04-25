#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <jrb.h>
#include <jval.h>
#include <dllist.h>
#include <sockettome.h>

typedef struct data {
  JRB rooms;
  int socket;
} Data;

typedef struct chat_room {
  char *name;
  int numClients;
  Dllist clients, messages;
  pthread_mutex_t *lock;
  pthread_cond_t *cond;
} Room;

typedef struct client {
  char *name, *room;
  FILE *read, *write;
  int fd;
  Data *data;
} Client;

void *main_function(void *arg);
void *room_function(void *arg);
void *client_function(void *arg);

int main (int argc, char **argv) {
  pthread_t tid;
  void *status;
  
  if (argc < 3) {
    fprintf(stderr, "Not enough arguments.\n");
    exit(0);
  }
  
  if (pthread_create(&tid, NULL, main_function, argv) != 0) {
    perror("pthread_create");
    exit(0);
  }
  
  if (pthread_join(tid, &status) != 0) {
    perror("pthread_join");
    exit(0);
  }

  return 0;
}

void *main_function(void *arg) {
  int i;
  int port;
  int fd;
  char **argv;
  Data *data;
  Room *r;
  Client *c;
  pthread_t tid;
  void *status;
  JRB tmp;
  //pthread_t tids[300];

  // malloc data
  data = malloc(sizeof(Data));
  data->rooms = make_jrb();

  // read all the rooms from the command line
  // allocate the room
  // insert into data->rooms
  // create a thread for the room
  argv = arg;
  for (i = 2; argv[i] != NULL; i++) {
    r = malloc(sizeof(Room));
    r->name = strdup(argv[i]);
    jrb_insert_str(data->rooms, argv[i], new_jval_v((void *) r));
    if (pthread_create(&tid, NULL, room_function, (void *) r) != 0) {
      perror("pthread_create");
      exit(0);
    }
    //tids[i-2] = tid;
  }

  // serve socket
  if(atoi(argv[1]) > 8000) {
    port = atoi(argv[1]);
    data->socket = serve_socket(port);
  } else {
    fprintf(stderr, "invalid port number");
  }

  // repeatedly accept a connection
  // when one is detected malloc the client and create a client thread
  while (1) {
    fd = accept_connection(data->socket);
    c = (Client *) malloc(sizeof(Client));
    c->fd = fd;
    c->data = data;
    if (pthread_create(&tid, NULL, client_function, (void *) c) != 0) {
      perror("pthread_create");
      exit(0);
    }
  }

  return NULL; 
}

void *room_function(void *arg) {
  Room *r;
  Dllist c, m;
  char *str;
  Client *client;

  r = arg;

  // initialize mutex and conditional variable and other things
  r->lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(r->lock, NULL);
  r->cond = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
  if(pthread_cond_init(r->cond, NULL) != 0) {
    fprintf(stderr, "ROOM THREAD: pthread_cond_init failed\n");
    exit(0);
  }
  r->clients = new_dllist();
  r->numClients = 0;

  pthread_mutex_lock(r->lock);
  while (1) {
    r->messages = new_dllist();
    pthread_cond_wait(r->cond, r->lock);
//    printf("%s waking up\n", r->name);
    if(!dll_empty(r->clients)) {
      if (r->clients->flink != NULL) {
        dll_traverse(c, r->clients) {
          dll_traverse(m, r->messages) {
            if (c != NULL) {
              str = m->val.s;
              client = c->val.v;
              fputs(str, client->write);
              fflush(client->write);
            }
          }
        }
      }
    }

    free_dllist(r->messages);
    pthread_mutex_unlock(r->lock);
  }

  return NULL;
}

void *client_function(void *arg) {
  Client *c, *p;
  Room *r;
  Dllist tmp;
  JRB room;
  char str[100];
  char message[1000];

  c = arg;
//  printf("CLIENT THREAD: I am in a client thread\n");
  
  // open the files for reading and writing
  c->read = fdopen(c->fd, "r");
  if (c->read == NULL) {
    perror("fdopen");
    exit(0);
  }
  c->write = fdopen(c->fd, "w");
  if (c->write == NULL) {
    perror("fdopen");
    exit(0);
  }

  // print all the chat rooms to the client
  if (fputs("Chat Rooms:\n\n", c->write) == EOF) {
//    printf("closing buffers\n");
    fclose(c->read);
    fclose(c->write);
    return NULL;
  }

  fflush(c->write);
//  printf("before traversing rooms\n");
  jrb_traverse(room, c->data->rooms) {
    r = room->val.v;
    fputs(r->name, c->write);
    fputs(":", c->write);

    if(!dll_empty(r->clients)) {
      printf("Clients is not empty\n");
      pthread_mutex_lock(r->lock);
      if (r->clients->flink != NULL) {
//      if (r->numClients > 0) {
        dll_traverse(tmp, r->clients) {
          if (tmp != NULL) { 
            if (tmp->val.v != NULL) {
              p = tmp->val.v;
              fputs(" ", c->write);
              fputs(p->name, c->write);
            }
          }      
        }
      }

      pthread_mutex_unlock(r->lock);
    }

    fputs("\n", c->write);
  }
  fputs("\nEnter your chat name (no spaces):\n", c->write);
  fflush(c->write);

  if (fgets(str, 100, c->read) == NULL) {
//    printf("closing buffers\n");
    fclose(c->read);
    fclose(c->write);
    return NULL;
  }
//  printf("read %s", str);
  c->name = strdup(str);
  c->name[strlen(c->name)-1] = '\0';

  // repeatedly prompt for a room until a valid one is given
  while (1) {
    fputs("Enter chat room:\n", c->write);
    fflush(c->write);
    if (fgets(str, 100, c->read) == NULL) {
//      printf("closing buffers\n");
      fclose(c->write);
      fclose(c->read);
      if(c->name != NULL) free(c->name);
      return NULL;
    }
    str[strlen(str)-1] = '\0';
    if (jrb_find_str(c->data->rooms, str) == NULL) {
      fputs("No chat room ", c->write);
      str[strlen(str)] = '.';
      fputs(str, c->write);
      fputs("\n", c->write);
    } else {
//      printf("Room %s was found\n", str);
      
      // set up pointer to the room
      room = jrb_find_str(c->data->rooms, str);
      r = room->val.v;
//      printf("Room: %s\n", r->name);
      c->room = r->name;

      // lock, then add the client to the clients list, 
      // and "... has joined" to the messages list, then signal and unlock
      pthread_mutex_lock(r->lock);
      dll_append(r->clients, new_jval_v((void *) c));
      r->numClients++;
      strcpy(message, c->name);
      strcat(message, " has joined\n");
//      printf("%s", message);
      dll_append(r->messages, new_jval_s(message));
      pthread_cond_signal(r->cond);
      pthread_mutex_unlock(r->lock);

      break;
    }
  }

  // send and recieve messages from the client until the client leaves
  while(1){
    if (fgets(str, 100, c->read) != NULL) {
      //if (
//      printf("Client said something\n"); 
      memset(message, 0, sizeof(message));
      strcat(message, c->name);
      strcat(message, ": ");
      strcat(message, str);
//      printf("%s", message);
      pthread_mutex_lock(r->lock);
      dll_append(r->messages, new_jval_s(message));
      pthread_cond_signal(r->cond);
      pthread_mutex_unlock(r->lock);
    } else {
//      printf("Client left\n");
      memset(message, 0, sizeof(message));
      strcat(message, c->name);
      strcat(message, " has left\n");
      pthread_mutex_lock(r->lock);
      dll_traverse(tmp, r->clients) {
        p = tmp->val.v;
        if (strcmp(p->name, c->name)) {
//          printf("found in the list\n");
          break;
        }
      }
      dll_delete_node(tmp);
      r->numClients--;
      dll_append(r->messages, new_jval_s(message));
      pthread_cond_signal(r->cond);
      pthread_mutex_unlock(r->lock);
      pthread_exit(NULL);
    }
  }

  return NULL;
}

