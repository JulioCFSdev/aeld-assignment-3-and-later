#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

void close_log(int level, const char *str){
    syslog(level, "%s", str);
    closelog();
}

int main(int argc, char *argv[])
{
    FILE* fptr;

    openlog(NULL, 0, LOG_USER);

    if (argc != 3){
	close_log(LOG_ERR, "Invalid number of arguments");
        return 1;
    }

    fptr = fopen(argv[1], "w");
    if (fptr == NULL) {
        close_log(LOG_ERR, "Error creating file");
	return 1;
    }

    if (fputs(argv[2], fptr) == EOF) {
        fclose(fptr);
        close_log(LOG_ERR, "Error writing to file");
        return 1;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);
    
    fclose(fptr);
    closelog();

    return 0;
}

