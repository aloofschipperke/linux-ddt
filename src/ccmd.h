struct builtin {
  char *name;
  char *arghelp;
  char *desc;
  void (*fn) (void);
};

void help(void);
void list_builtins(void);
int builtin(char *name);
