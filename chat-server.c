#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <jrb.h>
#include <jval.h>
#include <dllist.h>
#include </home/plank/cs360/include/sockettome.h>

typedef struct data {
  JRB rooms;
  int socket;
} Data;

typedef struct chat_room {
  char *name;
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
  
  if (argc < 4) {
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
  pthread_t tids[300];

  printf("MAIN THREAD: make the main thread\n");  

  // malloc data
  data = malloc(sizeof(Data));
  data->rooms = make_jrb();

  // read all the rooms from the command line
  // allocate the room
  // insert into data->rooms
  // create a thread for the room
  argv = arg;
  for (i = 3; argv[i] != NULL; i++) {
    printf("MAIN THREAD: reading %s\n", argv[i]);
    r = malloc(sizeof(Room));
    r->name = strdup(argv[i]);
    jrb_insert_str(data->rooms, argv[i], new_jval_v((void *) r));
    printf("MAIN THREAD: inserted %s into data->rooms\n", argv[i]);
    if (pthread_create(&tid, NULL, room_function, (void *) r) != 0) {
      perror("pthread_create");
      exit(0);
    }
    tids[i-3] = tid;
  }
/*
  for (i = 3; argv[i] != NULL; i++) {
    printf("joining pthread %d\n", tids[i-3]);  
    if (pthread_join(tids[i-3], &status) != 0) {
      perror("pthread_join");
      exit(0);
    }
  }
*/

  jrb_traverse(tmp, data->rooms) {
    printf("MAIN THREAD: room: %s\n", tmp->key); 
  }

  // serve socket
  if(atoi(argv[2]) > 8000) {
    port = atoi(argv[2]);
    printf("MAIN THREAD: serving port: %d\n", port);
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
    /*if (pthread_join(tid, &status) != 0) {
      perror("pthread_join");
      exit(0);
    }*/
  }

  return NULL; 
}

void *room_function(void *arg) {
  Room *r;
  Dllist c, m;
  char *str;
  Client *client;

  r = arg;
  printf("%s thread\n", r->name);

  // initialize mutex and conditional variable and other things
  r->lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t *));
  pthread_mutex_init(r->lock, NULL);
  r->cond = (pthread_cond_t *) malloc(sizeof(pthread_cond_t *));
  if(pthread_cond_init(r->cond, NULL) != 0) {
    printf("ROOM THREAD: pthread_cond_init failed\n");
    exit(0);
  }
  r->clients = new_dllist();
  printf("%s thread: I have made the lock and cond\n", r->name);


  while (1) {
    printf("in the while 1\n");
    r->messages = new_dllist();
    printf("%s locking...\n", r->name);
    pthread_cond_wait(r->cond, r->lock);
    dll_traverse(c, r->clients) {
      dll_traverse(m, r->messages) {
         str = m->val.s;
         client = c->val.v;
         fputs(str, client->write);
         fflush(client->write);
      }
    }

    free_dllist(r->messages);
    pthread_mutex_unlock(r->lock);
  }

  return NULL;
}

void *client_function(void *arg) {
  Client *c;
  Room *r;
  Dllist tmp;
  JRB room;

  c = arg;
  printf("CLIENT THREAD: I am in a client thread\n");
  
  // open the files for reading and writing
  c->read = fdopen(c->fd, "r");
  if (c->read == NULL) {
    perror("fdopen");
    exit(0);
  }
  //dup2(c->fd, 0);
  c->write = fdopen(c->fd, "w");
  if (c->write == NULL) {
    perror("fdopen");
    exit(0);
  }
  //dup2(c->fd, 1);

  // print all the chat rooms to the client
  fputs("Chat Rooms:\n\n", c->write);
  fflush(c->write);
  jrb_traverse(room, c->data->rooms) {
    r = room->val.v;
    fputs(r->name, c->write);
    fflush(c->write);
  }


  return NULL;
}
