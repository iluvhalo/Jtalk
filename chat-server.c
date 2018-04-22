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
  pthread_cond_t *condition;
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
    if (pthread_join(tid, &status) != 0) {
      perror("pthread_join");
      exit(0);
    }
  }

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
    if (pthread_join(tid, &status) != 0) {
      perror("pthread_join");
      exit(0);
    }
  }

  return NULL; 
}

void *room_function(void *arg) {
  Room *r;

  r = arg;
  printf("ROOM THREAD: I am in the room thread for room: %s\n", r->name);

  

  return NULL;
}

void *client_function(void *arg) {
  Client *c;

  c = arg;
  printf("CLIENT THREAD: I am in a client thread\n");

  return NULL;
}
