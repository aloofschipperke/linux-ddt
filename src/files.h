struct file {
  char *name;
  int devfd;
  int dirfd;
  int fd;
};

void files_init(void);
int syscommand(char *name, char *arg);
void delete_file(char *name);
