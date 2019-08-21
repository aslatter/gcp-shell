#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/*** forward declarations ***/

void parse_args(int argc, char **argv);
void usage();
void die(char *message);
void setup_tunnel();
void do_ssh();

/*** program data ***/

// options
char *prog_name = "gcp-shell";
char *zone = NULL;
int local_port = 2022;
char *instance = NULL;
char username_buff[100];
char *username = NULL;

// state
pid_t tunnel_child;
pid_t ssh_child;

/*** the program ***/

int main(int argc, char **argv) {
  parse_args(argc, argv);

  // do initial checks here
  // TODO - (i.e. do we have 'gcloud' and 'ssh')
  // TODO - check that args are not too long for buffers used further down

  // start tunnel
  pid_t tunnel_child = fork();
  if (tunnel_child == -1) {
    die("Error starting up process for tunnel");
  }
  if (!tunnel_child) {
    setup_tunnel();
    return 0; // unreachable
  }

  // wait for tunnel to set up (?!)
  sleep(2);

  // TODO no-hang wait for tunnel proc in case it failed

  // start child-process for ssh
  ssh_child = fork();
  if (ssh_child == -1) {
    perror("Error starting up process for ssh");
    kill(tunnel_child, SIGTERM);
    wait(NULL);
    exit(EXIT_FAILURE);
  } else if (ssh_child == 0) {
    do_ssh();
    return 0; // unreachable
  }

  // wait for processes to terminate
  while (true) {
    pid_t done_child = wait(NULL);
    if (done_child == -1) {
      if (errno == ECHILD) {
        // everything is good and we're done
        exit(EXIT_SUCCESS);
      }
      die("Error wrapping up");
    }

    // tunnel finished early - kill terminal
    if (done_child == tunnel_child && ssh_child) {
      kill(ssh_child, SIGTERM);
      tunnel_child = 0;
      continue;
    }

    // ssh done - kill tunnel
    if (done_child == ssh_child && tunnel_child) {
      kill(tunnel_child, SIGTERM);
      ssh_child = 0;
      continue;
    }
  }

  return 0;
}

void usage() {
  printf("Usage: %s [-z zone] [-p localPort=2020] [user@]instance\n\n",
         prog_name);
  printf("  Open an IAP tunnel to GCP compute <instance> on local-port "
         "<localPort>.\n");
  printf("  Then open an ssh connect, and close the tunnel when the conneciton "
         "is\n");
  printf("  complete.\n\n");

  printf("  The zone may be specified by the '-z' argument or by the usual\n");
  printf("  GCP configuration options.\n\n");
  exit(EXIT_FAILURE);
}

void parse_args(int argc, char **argv) {
  if (argc) {
    prog_name = argv[0];
  }

  int c;
  char *port_string = NULL;

  while ((c = getopt(argc, argv, "z:p:")) != -1) {
    switch (c) {
    case 'z':
      zone = optarg;
      printf("Got zone %s\n", zone);
      break;
    case 'p':
      port_string = optarg;
      break;
    case '?':
      usage();
    }
  }

  if (port_string) {
    if (sscanf(port_string, "%d", &local_port) != 1) {
      usage();
    }
  }

  // remaining arg is either 'instance' or 'user@instance'
  if (optind >= argc) {
    usage();
  }

  char *instance_arg = argv[optind];

  if ((instance = strchr(instance_arg, '@'))) {
    instance++;
    size_t user_len = instance - instance_arg - 1;
    if (user_len < sizeof(username_buff) + 1) {
      memcpy(username_buff, instance_arg, user_len);
      username_buff[user_len] = '\0';
      username = username_buff;
    } else {
      fprintf(stderr, "Username too long.\n");
      exit(EXIT_FAILURE);
    }
  } else {
    instance = instance_arg;
  }
}

void die(char *message) {
  perror(message);
  exit(EXIT_FAILURE);
}

void setup_tunnel() {

  // leave stderr connected to the terminal in
  // case something interesting happens
  int null = open("/dev/null", O_RDWR);
  if (null == -1) {
    die("Failed to open '/dev/null'");
  }
  dup2(null, STDIN_FILENO);
  dup2(null, STDOUT_FILENO);
  close(null);

  char *dest_port = "22";

  char bind_arg[200];

  if (snprintf(bind_arg, sizeof(bind_arg), "--local-host-port=localhost:%d",
               local_port) >= (ssize_t)sizeof(bind_arg)) {
    // TODO - print error to stderr: 'port' argument too large
    exit(EXIT_FAILURE);
  }

  // potential args: base, compute, start-iap-tunnel, instance, port,
  //   local-port, [zone], NULL
  char *args[8];
  int idx = 0;
  args[idx++] = "gcloud";
  args[idx++] = "compute";
  args[idx++] = "start-iap-tunnel";
  args[idx++] = instance;

  if (zone) {
    char zone_arg[200];
    if (snprintf(zone_arg, sizeof(zone_arg), "--zone=%s", zone) >=
        (ssize_t)sizeof(zone_arg)) {
      // TODO - print error
      exit(EXIT_FAILURE);
    }
  }
  args[idx++] = dest_port;
  args[idx++] = bind_arg;
  args[idx++] = NULL;

  execvp("gcloud", args);
  die("Error exec-ing 'gcloud'");
}

// do_ssh execs a call to ssh, leaving the terminal connected
// to stdout, stderr, and stdin.
void do_ssh() {

  char user_and_host[200];
  char port_string[200];

  if (username) {
    if (snprintf(user_and_host, sizeof(user_and_host), "%s@localhost",
                 username) >= (ssize_t)sizeof(user_and_host)) {
      // TODO - print error
      exit(EXIT_FAILURE);
    }
  } else {
    strcpy(user_and_host, "localhost");
  }

  if (snprintf(port_string, sizeof(port_string), "%d", local_port) >=
      (ssize_t)sizeof(port_string)) {
    // TODO - print error
    exit(EXIT_FAILURE);
  }

  char *args[] = {"ssh", user_and_host, "-p", port_string, NULL};
  execvp("ssh", args);
  die("Error exec-ing 'ssh'");
}
