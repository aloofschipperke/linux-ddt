struct file {
  char *name;
  int devfd;
  int dirfd;
  int fd;
};

void files_init(void);
struct file *findprog(char *name);
int syscommand(char *name, char *arg);
void delete_file(char *name);

extern struct file dsk;
extern struct file hsname;
extern struct file msname;
