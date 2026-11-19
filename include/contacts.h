#ifndef VILAOS_CONTACTS_H
#define VILAOS_CONTACTS_H

#include "types.h"

typedef struct {
    const char *name;
    const char *number;
} Contact;

int contacts_count(void);
const Contact *contacts_at(int idx);
const Contact *contacts_by_number(const char *num);
const Contact *contacts_search(const char *query);

#endif
