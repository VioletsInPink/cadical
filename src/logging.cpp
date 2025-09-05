#ifdef LOGGING

#include "internal.hpp"

namespace CaDiCaL {

void Logger::print_log_prefix (Internal *internal) {
  internal->print_prefix ();
  tout.magenta ();
  fputs ("LOG ", internal->logfile);
  tout.magenta (true);
  fprintf (internal->logfile, "%d ", internal->level);
  tout.normal ();
}

void Logger::log_empty_line (Internal *internal) {
  internal->print_prefix ();
  tout.magenta ();
  const int len = internal->prefix.size (), max = 78 - len;
  for (int i = 0; i < max; i++)
    fputc ('-', internal->logfile);
  fputc ('\n', internal->logfile);
  tout.normal ();
  fflush (internal->logfile);
}

void Logger::log (Internal *internal, const char *fmt, ...) {
  print_log_prefix (internal);
  tout.magenta ();
  va_list ap;
  va_start (ap, fmt);
  vfprintf (internal->logfile, fmt, ap);
  va_end (ap);
  fputc ('\n', internal->logfile);
  tout.normal ();
  fflush (internal->logfile);
}

// It is hard to factor out the common part between the two clause loggers,
// since they are also used in slightly different contexts.  Our attempt to
// do so were not more readable than the current version.  See the header
// for an explanation of the difference between the following two functions.

void Logger::log (Internal *internal, const Clause *c, const char *fmt,
                  ...) {
  print_log_prefix (internal);
  tout.magenta ();
  va_list ap;
  va_start (ap, fmt);
  vfprintf (internal->logfile, fmt, ap);
  va_end (ap);
  if (c) {
    if (c->redundant)
      fprintf (internal->logfile, " glue %d redundant", c->glue);
    else
      fprintf (internal->logfile, " irredundant");
    fprintf (internal->logfile, " size %d clause[%" PRId64 "]", c->size, c->id);
    if (c->moved)
      fprintf (internal->logfile, " ... (moved)");
    else {
      if (internal->opts.logsort) {
        vector<int> s;
        for (const auto &lit : *c)
          s.push_back (lit);
        sort (s.begin (), s.end (), clause_lit_less_than ());
        for (const auto &lit : s)
          fprintf (internal->logfile, " %d", lit);
      } else {
        for (const auto &lit : *c)
          fprintf (internal->logfile, " %d", lit);
      }
    }
  } else if (internal->level)
    fprintf (internal->logfile, " decision");
  else
    fprintf (internal->logfile, " unit");
  fputc ('\n', internal->logfile);
  tout.normal ();
  fflush (internal->logfile);
}

// Same as above, but for the global clause 'c' (which is not a reason).

void Logger::log (Internal *internal, const vector<int> &c, const char *fmt,
                  ...) {
  print_log_prefix (internal);
  tout.magenta ();
  va_list ap;
  va_start (ap, fmt);
  vfprintf (internal->logfile, fmt, ap);
  va_end (ap);
  if (internal->opts.logsort) {
    vector<int> s;
    for (const auto &lit : c)
      s.push_back (lit);
    sort (s.begin (), s.end (), clause_lit_less_than ());
    for (const auto &lit : s)
      fprintf (internal->logfile, " %d", lit);
  } else {
    for (const auto &lit : c)
      fprintf (internal->logfile, " %d", lit);
  }
  fputc ('\n', internal->logfile);
  tout.normal ();
  fflush (internal->logfile);
}

// Now for 'restore_clause' to avoid copying (without logging).

void Logger::log (Internal *internal,
                  const vector<int>::const_iterator &begin,
                  const vector<int>::const_iterator &end, const char *fmt,
                  ...) {
  print_log_prefix (internal);
  tout.magenta ();
  va_list ap;
  va_start (ap, fmt);
  vfprintf (internal->logfile, fmt, ap);
  va_end (ap);
  if (internal->opts.logsort) {
    vector<int> s;
    for (auto p = begin; p != end; p++)
      s.push_back (*p);
    sort (s.begin (), s.end (), clause_lit_less_than ());
    for (const auto &lit : s)
      fprintf (internal->logfile, " %d", lit);
  } else {
    for (auto p = begin; p != end; p++)
      fprintf (internal->logfile, " %d", *p);
  }
  fputc ('\n', internal->logfile);
  tout.normal ();
  fflush (internal->logfile);
}

// for LRAT proof chains

void Logger::log (Internal *internal, const vector<uint64_t> &c,
                  const char *fmt, ...) {
  print_log_prefix (internal);
  tout.magenta ();
  va_list ap;
  va_start (ap, fmt);
  vfprintf (internal->logfile, fmt, ap);
  va_end (ap);
  for (const auto &id : c)
    fprintf (internal->logfile, " %" PRIu64, id);
  fputc ('\n', internal->logfile);
  tout.normal ();
  fflush (internal->logfile);
}

} // namespace CaDiCaL

#endif
