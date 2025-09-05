#include "internal.hpp"

namespace CaDiCaL {

/*------------------------------------------------------------------------*/
#ifndef QUIET
/*------------------------------------------------------------------------*/

void Internal::print_prefix () { fputs (prefix.c_str (), logfile); }

void Internal::vmessage (const char *fmt, va_list &ap) {
#ifdef LOGGING
  if (!opts.log)
#endif
    if (opts.quiet)
      return;
  print_prefix ();
  vfprintf (logfile, fmt, ap);
  fputc ('\n', logfile);
  fflush (logfile);
}

void Internal::message (const char *fmt, ...) {
  va_list ap;
  va_start (ap, fmt);
  vmessage (fmt, ap);
  va_end (ap);
}

void Internal::message () {
#ifdef LOGGING
  if (!opts.log)
#endif
    if (opts.quiet)
      return;
  print_prefix ();
  fputc ('\n', logfile);
  fflush (logfile);
}

/*------------------------------------------------------------------------*/

void Internal::vverbose (int level, const char *fmt, va_list &ap) {
#ifdef LOGGING
  if (!opts.log)
#endif
    if (opts.quiet || level > opts.verbose)
      return;
  print_prefix ();
  vfprintf (logfile, fmt, ap);
  fputc ('\n', logfile);
  fflush (logfile);
}

void Internal::verbose (int level, const char *fmt, ...) {
  va_list ap;
  va_start (ap, fmt);
  vverbose (level, fmt, ap);
  va_end (ap);
}

void Internal::verbose (int level) {
#ifdef LOGGING
  if (!opts.log)
#endif
    if (opts.quiet || level > opts.verbose)
      return;
  print_prefix ();
  fputc ('\n', logfile);
  fflush (logfile);
}

/*------------------------------------------------------------------------*/

void Internal::section (const char *title) {
#ifdef LOGGING
  if (!opts.log)
#endif
    if (opts.quiet)
      return;
  if (stats.sections++)
    MSG ();
  print_prefix ();
  tout.blue ();
  fputs ("--- [ ", logfile);
  tout.blue (true);
  fputs (title, logfile);
  tout.blue ();
  fputs (" ] ", logfile);
  for (int i = strlen (title) + strlen (prefix.c_str ()) + 9; i < 78; i++)
    fputc ('-', logfile);
  tout.normal ();
  fputc ('\n', logfile);
  MSG ();
}

/*------------------------------------------------------------------------*/

void Internal::phase (const char *phase, const char *fmt, ...) {
#ifdef LOGGING
  if (!opts.log)
#endif
    if (opts.quiet || (!force_phase_messages && opts.verbose < 2))
      return;
  print_prefix ();
  fprintf (logfile, "[%s] ", phase);
  va_list ap;
  va_start (ap, fmt);
  vfprintf (logfile, fmt, ap);
  va_end (ap);
  fputc ('\n', logfile);
  fflush (logfile);
}

void Internal::phase (const char *phase, int64_t count, const char *fmt,
                      ...) {
#ifdef LOGGING
  if (!opts.log)
#endif
    if (opts.quiet || (!force_phase_messages && opts.verbose < 2))
      return;
  print_prefix ();
  fprintf (logfile, "[%s-%" PRId64 "] ", phase, count);
  va_list ap;
  va_start (ap, fmt);
  vfprintf (logfile, fmt, ap);
  va_end (ap);
  fputc ('\n', logfile);
  fflush (logfile);
}

/*------------------------------------------------------------------------*/
#endif // ifndef QUIET
/*------------------------------------------------------------------------*/

void Internal::warning (const char *fmt, ...) {
  fflush (logfile);
  terr.bold ();
  fputs ("cadical: ", logfile);
  terr.red (1);
  fputs ("warning:", logfile);
  terr.normal ();
  fputc (' ', logfile);
  va_list ap;
  va_start (ap, fmt);
  vfprintf (logfile, fmt, ap);
  va_end (ap);
  fputc ('\n', logfile);
  fflush (logfile);
}

/*------------------------------------------------------------------------*/

void Internal::error_message_start () {
  fflush (logfile);
  terr.bold ();
  fputs ("cadical: ", logfile);
  terr.red (1);
  fputs ("error:", logfile);
  terr.normal ();
  fputc (' ', logfile);
}

void Internal::error_message_end () {
  fputc ('\n', logfile);
  fflush (logfile);
  // TODO add possibility to use call back instead.
  exit (1);
}

void Internal::verror (const char *fmt, va_list &ap) {
  error_message_start ();
  vfprintf (logfile, fmt, ap);
  error_message_end ();
}

void Internal::error (const char *fmt, ...) {
  va_list ap;
  va_start (ap, fmt);
  verror (fmt, ap);
  va_end (ap); // unreachable
}

/*------------------------------------------------------------------------*/

void fatal_message_start () {
  fflush (stdout);
  terr.bold ();
  fputs ("cadical: ", stderr);
  terr.red (1);
  fputs ("fatal error:", stderr);
  terr.normal ();
  fputc (' ', stderr);
}

void fatal_message_end () {
  fputc ('\n', stderr);
  fflush (stderr);
  abort ();
}

void fatal (const char *fmt, ...) {
  fatal_message_start ();
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fatal_message_end ();
  abort ();
}

} // namespace CaDiCaL
