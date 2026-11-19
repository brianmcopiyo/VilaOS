#include "contacts.h"
#include "util.h"

#include <ctype.h>
#include <string.h>

static const Contact k_contacts[] = {
    {"Alice Chen", "555-0101"},
    {"Bob Okonkwo", "555-0102"},
    {"Carol Diaz", "555-0103"},
    {"Dave Park", "555-0104"},
    {"Eve Nakamura", "555-0105"},
};

#define CONTACT_N ((int)(sizeof(k_contacts) / sizeof(k_contacts[0])))

static void digits_only(const char *in, char *out, size_t n)
{
    size_t w = 0;
    if (!out || n == 0)
        return;
    for (; in && *in && w + 1 < n; in++) {
        if (isdigit((unsigned char)*in))
            out[w++] = *in;
    }
    out[w] = 0;
}

static int contains_ci(const char *hay, const char *needle)
{
    size_t nl = strlen(needle);
    if (nl == 0)
        return 0;
    for (; *hay; hay++) {
        size_t i = 0;
        while (i < nl && hay[i] && tolower((unsigned char)hay[i]) == tolower((unsigned char)needle[i]))
            i++;
        if (i == nl)
            return 1;
    }
    return 0;
}

int contacts_count(void)
{
    return CONTACT_N;
}

const Contact *contacts_at(int idx)
{
    if (idx < 0 || idx >= CONTACT_N)
        return NULL;
    return &k_contacts[idx];
}

const Contact *contacts_by_number(const char *num)
{
    char want[MAX_TEXT];
    digits_only(num, want, sizeof(want));
    if (!want[0])
        return NULL;
    for (int i = 0; i < CONTACT_N; i++) {
        char have[MAX_TEXT];
        digits_only(k_contacts[i].number, have, sizeof(have));
        if (str_starts(want, have))
            return &k_contacts[i];
    }
    return NULL;
}

const Contact *contacts_search(const char *query)
{
    if (!query || !query[0])
        return NULL;
    char qd[MAX_TEXT];
    digits_only(query, qd, sizeof(qd));
    for (int i = 0; i < CONTACT_N; i++) {
        if (contains_ci(k_contacts[i].name, query))
            return &k_contacts[i];
        if (qd[0]) {
            char have[MAX_TEXT];
            digits_only(k_contacts[i].number, have, sizeof(have));
            if (strstr(have, qd))
                return &k_contacts[i];
        }
    }
    return NULL;
}
