#include <errno.h>
#include <poll.h>
#include <seccomp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/pidfd.h>
#include <sys/poll.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <wait.h>

#define TARGET_NOTIFY_FD 100

void run_child(char *const argv[]) {
  // Allow all syscalls by default
  scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);

  if (ctx == NULL) {
    perror("seccomp_init");
    exit(EXIT_FAILURE);
  }

  if (seccomp_rule_add(ctx, SCMP_ACT_NOTIFY, SCMP_SYS(socket), 0) ||
      seccomp_rule_add(ctx, SCMP_ACT_NOTIFY, SCMP_SYS(bind), 0) ||
      seccomp_rule_add(ctx, SCMP_ACT_NOTIFY, SCMP_SYS(recvfrom), 0) ||
      seccomp_rule_add(ctx, SCMP_ACT_NOTIFY, SCMP_SYS(sendto), 0)) {
    perror("seccomp_rule_add");
    exit(EXIT_FAILURE);
  }

  // Load the filter in the kernel
  if (seccomp_load(ctx) != 0) {
    perror("seccomp_load");
    seccomp_release(ctx);
    exit(EXIT_FAILURE);
  }

  // Create the seccomp fd and move it to TARGET_NOTIFY_FD
  // Since it's a fixed value, the supervisor can retrieves it
  // in its view and read from it.
  int notify_fd = seccomp_notify_fd(ctx);
  if (notify_fd < 0 || dup2(notify_fd, TARGET_NOTIFY_FD) < 0) {
    perror("seccomp_notify_fd || dup2");
    exit(EXIT_FAILURE);
  }

  // If the old fd is not the same as the new one, close it. We'll keep the new
  // one for now.
  if (notify_fd != TARGET_NOTIFY_FD) {
    close(notify_fd);
  }

  // The filter has been loaded in the kernel, the notify fd created, so the
  // memory associated to the context can be freed
  seccomp_release(ctx);

  // We stop this process, the supervisor will catch this and will resume it
  // using SIGCONT when it has retrieved the notify fd
  raise(SIGSTOP);

  // The parent has the fd, we close it on this process so we don't answer
  // to our own notifications.
  close(TARGET_NOTIFY_FD);

  // At this point every syscalls matching the filter will be intercepted
  printf("seccomp filter installed");

  execv(argv[0], argv);

  exit(EXIT_SUCCESS);
}

#define emulated_syscall(request, response, syscall, handler)                  \
  if (request->data.nr == SCMP_SYS(syscall)) {                                 \
    handler(request, response);                                                \
  }

// Emulated syscalls impl

void bp_socket(struct seccomp_notif *request,
               struct seccomp_notif_resp *response) {
  printf("Received syscall: socket");
  response->error = -ENOSYS;
}

void bp_bind(struct seccomp_notif *request,
             struct seccomp_notif_resp *response) {
  printf("Received syscall: bind");
  response->error = -ENOSYS;
}

void bp_recvfrom(struct seccomp_notif *request,
                 struct seccomp_notif_resp *response) {
  printf("Received syscall: recvfrom");
  response->error = -ENOSYS;
}

void bp_sendto(struct seccomp_notif *request,
               struct seccomp_notif_resp *response) {
  printf("Received syscall: sendto");
  response->error = -ENOSYS;
}

void run_supervisor(int notify_fd) {
  struct seccomp_notif *request;
  struct seccomp_notif_resp *response;

  if (seccomp_notify_alloc(&request, &response) != 0) {
    perror("seccomp_notify_alloc");
    return;
  }

  while (true) {
    // POLLIN means there's data to be read
    struct pollfd pfd = {.fd = notify_fd, .events = POLLIN};

    // We wait for the next even
    if (poll(&pfd, 1, -1) < 0) {
      // A signal arrived while waiting on the supervisor process
      if (errno == EINTR) {
        continue;
      }

      // If an error occured we exit
      break;
    }

    // When the child closes the stream we break the loop
    if (pfd.events & POLLHUP)
      break;

    // If the message parsing fails we simply ignore this packet
    if (seccomp_notify_receive(notify_fd, request) != 0) {
      continue;
    }

    // Reset response structure
    memset(response, 0, sizeof(*response));
    response->id = request->id;

    emulated_syscall(request, response, socket, bp_socket);
    emulated_syscall(request, response, bind, bp_bind);
    emulated_syscall(request, response, recvfrom, bp_recvfrom);
    emulated_syscall(request, response, sendto, bp_sendto);

    if (seccomp_notify_respond(notify_fd, response) != 0) {
      perror("seccomp_notify_respond");
    }
  }

  seccomp_notify_free(request, response);
}

int main(int argc, char *argv[]) {
  // Start the child process
  pid_t child_pid = fork();
  if (child_pid == -1) {
    perror("fork");
    return EXIT_FAILURE;
  }
  if (child_pid == 0) {
    run_child(&argv[1]);
  }

  int status;
  if (waitpid(child_pid, &status, WUNTRACED) < 0 || !WIFSTOPPED(status)) {
    fprintf(stderr, "child process did not stop as expected");
    return EXIT_FAILURE;
  }

  // Get a handle of our child process
  int pidfd = pidfd_open(child_pid, 0);
  if (pidfd < 0) {
    perror("pidfd_open");
    exit(EXIT_FAILURE);
  }

  // Retrieve a fd living in the child process for this process
  int notify_fd = pidfd_getfd(pidfd, TARGET_NOTIFY_FD, 0);
  if (notify_fd < 0) {
    perror("pidfd_getfd");
    exit(EXIT_FAILURE);
  }
  close(pidfd); // We don't need that now that we have the notify_fd

  // Tell the child process to continue
  kill(child_pid, SIGCONT);

  run_supervisor(notify_fd);
  close(notify_fd);

  return EXIT_SUCCESS;
}
