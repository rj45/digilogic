/*

microgettext - A stupidly minimalist clone of GNU libintl
Copyright (C) 2021 Romain GARBI

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/
#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MICROGETTEXT_DOMAIN_MAX 100
#define MICROGETTEXT_DEFAULT_BASEDIR "."

struct domain_t {
  char *name;
  void *buf;
  const char **strings;
  const char **translations;
  size_t nb;
  struct domain_t *next;
};

struct domain_basedir_t {
  char name[100];
  char *basedir;
  struct domain_basedir_t *next;
};

static struct domain_t *domains = NULL;
static struct domain_basedir_t *basedirs = NULL;
static char current_domain[128] = "";

static const int locale_categories[] = {LC_ALL,      LC_COLLATE, LC_CTYPE,
                                        LC_MONETARY, LC_NUMERIC, LC_TIME};

static const char *locale_strings[] = {"LC_MESSAGES", "LC_COLLATE", "LC_CTYPE",
                                       "LC_MONETARY", "LC_NUMERIC", "LC_TIME"};

static const char *iso639[] = {
  "ab", "abkhazian",     "aa", "afar",        "af", "afrikaans",
  "ak", "akan",          "sq", "albanian",    "am", "amharic",
  "ar", "arabic",        "an", "aragonese",   "hy", "armenian",
  "as", "assamese",      "av", "avaric",      "ae", "avestan",
  "ay", "aymara",        "az", "azerbaijani", "bm", "bambara",
  "ba", "bashkir",       "eu", "basque",      "eu", "basque",
  "be", "belarusian",    "bn", "bengali",     "bh", "bihari",
  "bi", "bislama",       "nb", "bokmal",      "bs", "bosnian",
  "br", "breton",        "bg", "bulgarian",   "my", "burmese",
  "ca", "catalan",       "km", "central",     "ch", "chamorro",
  "ce", "chechen",       "ny", "chichewa",    "zh", "chinese",
  "cu", "church",        "cv", "chuvash",     "kw", "cornish",
  "co", "corsican",      "cr", "cree",        "hr", "croatian",
  "cs", "czech",         "cs", "czech",       "da", "danish",
  "dv", "divehi",        "nl", "dutch",       "nl", "dutch;",
  "dz", "dzongkha",      "en", "english",     "eo", "esperanto",
  "et", "estonian",      "ee", "ewe",         "fo", "faroese",
  "fj", "fijian",        "fi", "finnish",     "fr", "french",
  "fr", "french",        "fy", "frisian",     "ff", "fulah",
  "gd", "gaelic",        "gl", "galician",    "lg", "ganda",
  "ka", "georgian",      "de", "german",      "de", "german",
  "el", "greek",         "gn", "guarani",     "gu", "gujarati",
  "ht", "haitian",       "ha", "hausa",       "he", "hebrew",
  "hz", "herero",        "hi", "hindi",       "ho", "hiri",
  "hu", "hungarian",     "is", "icelandic",   "io", "ido",
  "ig", "igbo",          "id", "indonesian",  "ia", "interlingua",
  "ie", "interlingue",   "iu", "inuktitut",   "ik", "inupiaq",
  "ga", "irish",         "it", "italian",     "ja", "japanese",
  "jv", "javanese",      "kl", "kalaallisut", "kn", "kannada",
  "kr", "kanuri",        "ks", "kashmiri",    "kk", "kazakh",
  "ki", "kikuyu;",       "rw", "kinyarwanda", "ky", "kirghiz",
  "kv", "komi",          "kg", "kongo",       "ko", "korean",
  "kj", "kuanyama",      "ku", "kurdish",     "lo", "lao",
  "la", "latin",         "lv", "latvian",     "li", "limburgan",
  "ln", "lingala",       "lt", "lithuanian",  "lu", "luba-katanga",
  "lb", "luxembourgish", "mk", "macedonian",  "mk", "macedonian",
  "mg", "malagasy",      "ms", "malay",       "ms", "malay",
  "ml", "malayalam",     "mt", "maltese",     "gv", "manx",
  "mi", "maori",         "mr", "marathi",     "mh", "marshallese",
  "mn", "mongolian",     "na", "nauru",       "nv", "navajo",
  "nd", "ndebele",       "ng", "ndonga",      "ne", "nepali",
  "se", "northern",      "no", "norwegian",   "oc", "occitan",
  "oj", "ojibwa",        "or", "oriya",       "om", "oromo",
  "os", "ossetian;",     "pi", "pali",        "pa", "panjabi",
  "fa", "persian",       "fa", "persian",     "pl", "polish",
  "pt", "portuguese",    "ps", "pushto",      "qu", "quechua",
  "ro", "romanian",      "rm", "romansh",     "rn", "rundi",
  "ru", "russian",       "sm", "samoan",      "sg", "sango",
  "sa", "sanskrit",      "sc", "sardinian",   "sr", "serbian",
  "sn", "shona",         "ii", "sichuan",     "sd", "sindhi",
  "si", "sinhala",       "sk", "slovak",      "sk", "slovak",
  "sl", "slovenian",     "so", "somali",      "st", "sotho",
  "es", "spanish;",      "su", "sundanese",   "sw", "swahili",
  "ss", "swati",         "sv", "swedish",     "tl", "tagalog",
  "ty", "tahitian",      "tg", "tajik",       "ta", "tamil",
  "tt", "tatar",         "te", "telugu",      "th", "thai",
  "bo", "tibetan",       "bo", "tibetan",     "ti", "tigrinya",
  "to", "tonga",         "ts", "tsonga",      "tn", "tswana",
  "tr", "turkish",       "tk", "turkmen",     "tw", "twi",
  "ug", "uighur;",       "uk", "ukrainian",   "ur", "urdu",
  "uz", "uzbek",         "ve", "venda",       "vi", "vietnamese",
  "vo", "volap�k",       "wa", "walloon",     "cy", "welsh",
  "cy", "welsh",         "wo", "wolof",       "xh", "xhosa",
  "yi", "yiddish",       "yo", "yoruba",      "za", "zhuang",
  "zu", "zulu"};

/* A macro to check that the required index of item is within
   the mo file buffer */
#define microgettext_check_bounds(len, item, index)                            \
  ((len) < (sizeof(item) * ((index) + 1)))

/* Kind of poor programming, but on some versions of windows,
   fseek can't get through EOT characters, so we are looping through
   the entire file to determine the entire length of the file*/
size_t microgettext_filelen(FILE *restrict f) {
  size_t ret = 0;
  int pos;
  char tmp;

  /* On some platforms (say windows for example), you don't
     necessarily start at index 0 */
  pos = ftell(f);

  while (fread(&tmp, sizeof(char), 1, f) == 1)
    ret++;

  pos = fseek(f, SEEK_SET, pos);

  return ret;
}

/* loads a file *.mo file in memory
    - f : A path to the mo file to process
    - l : A pointer to return the allocated file size
    - return : A pointer to a buf containing a mapping of the file */
void *microgettext_loadfile(const char *restrict f, size_t *restrict l) {
  FILE *fp;
  size_t len;
  void *p = NULL;

  /* open the file in binary mode */
  if ((fp = fopen(f, "rb")) == NULL)
    goto error;

  /* Get the length of the file */
  len = microgettext_filelen(fp);

  /* malloc a buffer that's the size of the file */
  if (
    ((p = malloc(len * sizeof(char))) == NULL) ||
    (fread(p, sizeof(char), len, fp) != len))
    goto error;

  fclose(fp);
  *l = len;

  return p;

error:
  if (fp != NULL)
    fclose(fp);
  if (p != NULL)
    free(p);
  return NULL;
}

/* Makes an array of strings from an mo file (please refer to gnu mo files
   documentation for more infos on the format)
      - array : An array to receive pointers to the strings
      - nitems : Number of items to be looked for
      - offset : The offset at which the data is stored from the start of the
        file
      - buf : A buffer containing a mapping for the mofile
      - len : The length of buf in bytes */
int microgettext_make_array(
  const char **array, size_t nitems, int offset, void *buf, size_t len) {
  int *int_buf, i = 0;
  char *char_buf = buf;

  /* Just check there's enough room for the whole table */
  if (microgettext_check_bounds(len, char, offset + 2 * nitems * sizeof(int)))
    return -1;

  /* Use this cast because offset*sizeof(char) is not always a multiple of
     sizeof(int) */
  int_buf = (int *)((void *)char_buf + offset);

  for (i = 0; i < nitems; i++) {

    /* Just check that there is enough room in the file to find
       the strings */
    if (microgettext_check_bounds(len, char, int_buf[2 * i + 1]))
      return -1;

    array[i] = buf + int_buf[2 * i + 1];
  }

  return 0;
}

/* Loads a mofile
    - *d : A pointer to the domain_t struct to be filled
    - *f : A path to the file to open */
int microgettext_load_mofile(
  struct domain_t *restrict d, const char *restrict f) {
  void *buf;
  int *int_buf;
  const char **strings = NULL, **translations = NULL;
  size_t len, /* length of the file buffer */
    nb /* number of strings in the buffer */;

  /* Get a buffer copy of the file */
  if ((buf = microgettext_loadfile(f, &len)) == NULL)
    goto error;

  int_buf = (int *)buf;

  /* Check that we've got the magic number at position 0 */
  if (
    microgettext_check_bounds(len, int, 4) && (int_buf[0] != 0x950412de) &&
    (int_buf[0] != 0xde120495))
    goto error;

  nb = int_buf[2];

  /* Kind of "try" / "catch" statement */
  if (/* Malloc strings and translation tables */
      (strings = malloc(len * sizeof(char *))) == NULL ||
      (translations = malloc(len * sizeof(char *))) == NULL
      /* Fill the strings and translation tables */
      || microgettext_make_array(strings, nb, int_buf[3], buf, len) ||
      microgettext_make_array(translations, nb, int_buf[4], buf, len))
    goto error;

  /* If loading is successful, only then apply the values to the string */
  d->buf = buf;
  d->strings = strings;
  d->translations = translations;
  d->nb = nb;

  return 0;
error:
  if (buf)
    free(buf);
  if (strings)
    free(strings);
  if (translations)
    free(translations);

  return -1;
}

/* A wrapper of over c standard bsearch to get array index instead of
   pointer
        - *key : a pointer to the item to be searched
        - *base : An array of items where to find an occurence of
          *key.
        - num : Number of items in the *base array
        - size : Size of each of *base element
        - *compar : A comparison function
*/
int microgettext_get_index(
  void *key, void *base, size_t num, size_t size,
  int (*compar)(const void *, const void *)) {
  void *ret;

  if ((ret = bsearch(key, base, num, size, compar))) {

    return (ret - base) / size;

  } else
    return -1;
}

/* A function to lowercase a *str */
void microgettext_lower(char *str) {
  while (*str) {
    *str = tolower(*str);
    str++;
  }
}

/* Allocate a duplicate of *str */
char *microgettext_strdup(const char *str) {
  char *p;

  if ((p = malloc(strlen(str) + 1)) == NULL)
    return NULL;

  strcpy(p, str);

  return p;
}

/* Fancy comparing function for bsearch in the i639 array */
int microgettext_lang_cmp(const void *p1, const void *p2) {
  char **s1 = (char **)p1, **s2 = (char **)p2;

  /* s1[0] and s2[0] are iso639 associated with lang */
  return strcmp(s1[1], s2[1]);
}

/* A function to convert locale name to iso639 2 letter lang format
    - cat : The locale category to be used
    - *loc : A pointer to a string that will receive the resulting
      lang name
    - size : The length of the buffer pointed to by *loc
*/
void microgettext_get_lang(int cat, char *restrict loc, size_t size) {
  char *key[] = {NULL, loc}, *p;
  int i;

  snprintf(loc, size, "%s", setlocale(cat, NULL));

  /* truncate at the first '_' */
  if ((p = strchr(loc, '_')))
    *p = '\0';

  microgettext_lower(loc);

  /* Check if loc matches an english language name and convert it to
     2 letters format */
  if (
    (i = microgettext_get_index(
       key, iso639, sizeof(iso639) / (2 * sizeof(char *)), sizeof(char *) * 2,
       microgettext_lang_cmp)) != -1) {

    strncpy(loc, iso639[i * 2], size);
    loc[size - 1] = '\0';
  }
}

/* Produces the *.mo file path associated with a domain and a category
    - *mo : A pointer to a buffer that will receive the path of the
      mo file to load
    - *do : A pointer to a string containing the domain name
    - cat : The selected locale category
    - size : The length of the *mo buffer in chars */
char *microgettext_make_domain_path(const char *restrict d, int cat) {
  const char *cat_str = "LC_MESSAGES", *domain_basedir;
  struct domain_basedir_t *item = basedirs;
  char lstr[100], *mo;
  size_t len;
  int i;

  /* loop through transcriptions to find the appropriate category */
  for (i = 0; i < sizeof(locale_categories); i++)
    if (cat == locale_categories[i])
      cat_str = locale_strings[i];

  microgettext_get_lang(cat, lstr, sizeof(lstr));

  /* try to get the appropriate basedir */
  while (item && strcmp(item->name, d))
    item = item->next;

  domain_basedir = item ? item->basedir : NULL;

  /* compute the length of the string to return */
  len = strlen(lstr) + sizeof("/") + strlen(cat_str) + sizeof("/") + strlen(d) +
        sizeof(".mo") + 1;

  if (domain_basedir)
    len += strlen(domain_basedir) + sizeof("/");

  if ((mo = malloc(len))) {
    if (domain_basedir)
      snprintf(mo, len, "%s/%s/%s/%s.mo", domain_basedir, lstr, cat_str, d);
    else
      snprintf(mo, len, "%s/%s/%s.mo", lstr, cat_str, d);
  }

  return mo;
}

/* A comparing function for bsearch-ing the untranslated strings */
int microgettext_strcmp(const void *p1, const void *p2) {
  char **s1 = (char **)p1, **s2 = (char **)p2;

  return strcmp(*s1, *s2);
}

/* Translates messages given a domain name and a locale category
    - *d : A pointer to a string specifying the domain to be used
    - *msg : A pointer to the string to translate
    - cat : The locale category selected */
const char *
dcgettext(const char *restrict d, const char *restrict msg, int cat) {
  struct domain_t *item = domains;
  int malloced = 0, i;
  char *moname = NULL, **key = (char **)&msg;

  if ((moname = microgettext_make_domain_path(d, cat)) == NULL)
    goto error;

  printf("loading \"%s\"\n", moname);

  /* loop through domains to find the right domain */
  while (item && strcmp(item->name, moname))
    item = item->next;

  /* open the domain if not existing */
  if (item == NULL) {
    if ((item = malloc(sizeof(struct domain_t))) == NULL)
      goto error;

    malloced = 1;

    /* copy the name of the domain */
    item->name = moname;

    /* add domains to the list */
    item->next = domains ? domains : NULL;
    domains = item;

    /* load the content of the mo file */
    if (microgettext_load_mofile(item, moname))
      goto error;
  }

  if (
    (i = microgettext_get_index(
       key, item->strings, item->nb, sizeof(char **), microgettext_strcmp)) !=
    -1)
    return item->translations[i];

  return msg;

error:
  if (moname)
    free(moname);

  if (malloced) {
    domains = item->next;
    free(item);
  }

  return msg;
}

/* Translates messages given a domain name
    - *dmn : A pointer to a string specifying the domain to be used
    - *msg : A pointer to the string to translate */
const char *dgettext(const char *restrict dmn, const char *restrict msg) {
  return dcgettext(dmn, msg, LC_ALL);
}

/* Translates messages
    - *msg : A pointer to the string to translate */
const char *gettext(const char *restrict msg) {
  return dcgettext(*current_domain ? current_domain : "C", msg, LC_ALL);
}

/* Sets a default domain for gettext() function */
char *textdomain(const char *dmn) {
  if (dmn) {

    strncpy(current_domain, dmn, sizeof(current_domain));
    current_domain[sizeof(current_domain) - 1] = '\0';
  }

  return current_domain;
}

char *bindtextdomain(const char *restrict dmn, const char *restrict dir) {
  struct domain_basedir_t *item = basedirs;

  while (item && strcmp(item->name, dmn))
    item = item->next;

  if (dir) {

    if (item == NULL) {

      /* allocate and fill a new domain basedir */
      if ((item = malloc(sizeof(struct domain_basedir_t))) == NULL)
        goto error;

      strncpy(item->name, dmn, sizeof(item->name));
      item->name[sizeof(item->name) - 1] = '\0';
      item->next = basedirs ? basedirs : NULL;

      basedirs = item;
    } else
      free(item->basedir);

    /* fill the basedir value */
    item->basedir = microgettext_strdup(dir);
  }

  return item ? item->basedir : MICROGETTEXT_DEFAULT_BASEDIR;

error:
  return NULL;
}

/* Do nothing. this is a stub for compatibility */
char *bind_textdomain_codeset(const char *domainname, const char *codeset) {
  return (char *)codeset;
}