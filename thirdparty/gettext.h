/*

microgettext - A stupidly minimalist implementation of GNU libintl
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
#ifndef MICROGETTEXT_H_INC
#define MICROGETTEXT_H_INC

/* Translates messages given a domain name and a locale category
    - *d : A pointer to a string specifying the domain to be used
    - *msg : A pointer to the string to translate
    - cat : The locale category selected */
const char *
dcgettext(const char *restrict d, const char *restrict msg, int cat);

/* Translates messages given a domain name
    - *dmn : A pointer to a string specifying the domain to be used
    - *msg : A pointer to the string to translate */
const char *dgettext(const char *restrict dmn, const char *restrict msg);

/* Translates messages
    - *msg : A pointer to the string to translate */
const char *gettext(const char *restrict msg);

/* Sets a default domain for gettext() function */
char *textdomain(const char *dmn);

/* */
char *bindtextdomain(const char *restrict dmn, const char *restrict dir);

/* Do nothing. this is a stub for compatibility */
char *bind_textdomain_codeset(const char *domainname, const char *codeset);

#define _(msg) gettext(msg)

#endif // MICROGETTEXT_H_INC