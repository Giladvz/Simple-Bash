#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

void close_zombies(int sig) {
    if (waitpid(-1,NULL,WNOHANG)<0) {
        if (errno != EINTR && errno != ECHILD) {
            fprintf(stderr,"%s\n", strerror(errno));
	    // Error handling 2)
	    exit(1);
        }
    }
}

int prepare(void){
    // Take care of zombies
    struct sigaction child;
    memset(&child,0,sizeof(child));
    child.sa_handler = close_zombies;
    child.sa_flags = SA_RESTART;
    sigaction(SIGCHLD,&child,NULL);

    if (signal(SIGINT,SIG_IGN)< 0) {
        fprintf(stderr,"%s\n", strerror(errno));
        return 1;
    }
    return 0;

}

// Pipe can be anywhere in the list. Check each word.
int hasPipe(int count,char** arglist) {
    for (int i=1; i < count - 1; i++) {
        if (strcmp(arglist[i],"|") == 0) {
            // This way we know which word is the pipe
            return i;
        }
    }
    return 0;
}

int regular_command(char** arglist) {
    int wait_return_status = -1;
    int fork_return_status = -1;
    // Sets child code
    if ((fork_return_status = fork()) == 0) {
        if (signal(SIGINT,SIG_DFL)< 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
        if (execvp(arglist[0],arglist) < 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
    }
    // Problem with fork, returns 0
    else if (fork_return_status < 0) {
        fprintf(stderr,"%s\n", strerror(errno));
        return 0;
    }
    // Sets parent code
    else {
        wait_return_status = waitpid(fork_return_status,NULL,0);
        if (wait_return_status == -1) {
            if (errno != ECHILD && errno != EINTR) {
                fprintf(stderr,"%s\n", strerror(errno));
                return 0;
            }
        }
    }
    return 1;
}

int background_command(char** arglist,int count) {
    int fork_return_status = -1;
    // Sets child code
    if ((fork_return_status = fork()) == 0) {
        // Removes & for execvp command
        arglist[count-1] = NULL;
        // Problem with execvp, exits child process
        if (execvp(arglist[0],arglist) < 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
    }
    // Problem with fork, returns 0
    else if (fork_return_status < 0) {
        fprintf(stderr,"%s\n", strerror(errno));
        return 0;
    }
    return 1;
}

int output_redirect_command(char** arglist,int count) {
    int wait_return_status = -1;
    int fork_return_status = -1;

    // Sets child code
    if ((fork_return_status = fork()) == 0) {
        if (signal(SIGINT,SIG_DFL)< 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
        int outfile = open(arglist[count-1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        arglist[count-1] = NULL;
        arglist[count-2] = NULL;
        // Problem with open, exits child
        if (outfile < 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
        // Problem with redirection, exits child
        if (dup2(outfile,1) < -1) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
        // Problems with close, exits child
        if (close(outfile) < -1){
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
        // Problems with execvp, exits child
        if (execvp(arglist[0],arglist) < 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
    }
    // Problem with fork, returns 0
    else if (fork_return_status < 0) {
        fprintf(stderr,"%s\n", strerror(errno));
        return 0;
    }
    // Sets parent code
    else {
        wait_return_status = waitpid(fork_return_status,NULL,0);
        if (wait_return_status == -1) {
            if (errno != ECHILD && errno != EINTR) {
                fprintf(stderr,"%s\n", strerror(errno));
                return 0;
            }
        }
    }
    return 1;
}

int pipe_command(char** arglist,int location) {
    int fds[2];
    int wait_return_status = -1;
    int fork_return_status = -1;
    int fork_return_status2 = -1;
    arglist[location]=NULL;

    // Pipe failed, but we want the shell to continue and the child process
    // to exit, returning doesn't create the children so everything works as expected.
    // I could have created the second child under the first, but I wanted both children to be under the shell.
    if (pipe(fds) == -1) {
        fprintf(stderr,"%s\n", strerror(errno));
        return 1;
    }

    // First child writes
    if ((fork_return_status = fork()) == 0) {
        if (signal(SIGINT,SIG_DFL)< 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
        if (close(fds[0]) < 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
        if (dup2(fds[1],1) < 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
        if (close(fds[1]) < 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
        if (execvp(arglist[0],arglist) < 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
    }
    // Problem with fork, returns 0
    else if (fork_return_status < 0) {
        fprintf(stderr,"%s\n", strerror(errno));
        return 0;
    }

    // Second child reads
    if ((fork_return_status2 = fork()) == 0) {
        if (signal(SIGINT,SIG_DFL)< 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
        close(STDIN_FILENO);
        if (close(fds[1]) < 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
        if (dup2(fds[0],0) < 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
        if (close(fds[0]) < 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
        if (execvp(arglist[location+1],&arglist[location+1]) < 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            exit(1);
        }
    }
    // Problem with fork, returns 0
    else if (fork_return_status < 0) {
        fprintf(stderr,"%s\n", strerror(errno));
        return 0;
    }
    // Sets parent code
    else {
        if (close(fds[1]) < 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            return 1;
        }
        if (close(fds[0]) < 0) {
            fprintf(stderr,"%s\n", strerror(errno));
            return 1;
        }

        wait_return_status = waitpid(fork_return_status,NULL,0);
        if (wait_return_status == -1) {
            if (errno != ECHILD && errno != EINTR) {
                fprintf(stderr, "%s\n", strerror(errno));
                return 0;
            }
        }

	    wait_return_status = waitpid(fork_return_status2,NULL,0);
        if (wait_return_status == -1) {
            if (errno != ECHILD && errno != EINTR) {
                fprintf(stderr, "%s\n", strerror(errno));
                return 0;
            }
        }
    }
    return 1;
}

int process_arglist(int count, char** arglist){
    int pipe_location;
    // Takes care of input of background command
    if ((count > 1) && (strcmp(arglist[count-1],"&")==0)){
        return background_command(arglist,count);
    }
    // Using > means that at least 3 words are used
    else if ((count > 2) && (strcmp(arglist[count-2],">")==0)) {
        return output_redirect_command(arglist,count);
    }
    // Takes care of commands with pipe
    else if ((count > 2) && ((pipe_location = hasPipe(count,arglist)) != 0)){
        return pipe_command(arglist,pipe_location);
    }
    else {
        return regular_command(arglist);
    }
}

int finalize(void){
    return 0;
}
