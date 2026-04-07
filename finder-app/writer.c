#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <file> <text>\n", argv[0]);
    return EXIT_FAILURE;
  }
  
  const char *filename = argv[1];
  const char *text = argv[2];
  
  openlog("myutility", LOG_PID | LOG_CONS, LOG_USER);
  
  syslog(LOG_DEBUG, "Writing '%s' to %s", text, filename);
  
  FILE *fp = fopen(filename, "w");
  if (fp == NULL) {
    syslog(LOG_ERR, "Failed to open file %s: %s", filename, strerror(errno));
    closelog();
    return EXIT_FAILURE;
  }
  
  if (fprintf(fp, "%s\n", text) < 0) {
    syslog(LOG_ERR, "Failed to write to file %s: %s", filename, strerror(errno));
    fclose(fp);
    closelog();
    return EXIT_FAILURE;
  }
  
  if (fclose(fp) != 0) {
    syslog(LOG_ERR, "Failed to close file %s: %s", filename, strerror(errno));
    closelog();
    return EXIT_FAILURE;
  }
  
  closelog();
  
  return EXIT_SUCCESS;
}
