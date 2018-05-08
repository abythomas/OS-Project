#ifndef _FOPS_H
#define _FOPS_H

extern int tym_currentime(struct seq_file *m, void *p);
extern int tym_fn(struct seq_file *m, void *p);
extern int tym_timer(struct seq_file *m, void *p);
extern int tym_tasklet(struct seq_file *m, void *p);

#endif
