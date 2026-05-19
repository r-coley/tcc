#include <stdio.h>

/* This checks various ways of dead code inside if statements
   where there are non-obvious ways of how the code is actually
   not dead due to reachable by labels.  */
extern int fprintf (FILE *,const char *, ...);
static void kb_wait_1(void)
{
  unsigned long timeout = 2;
  do {
      /* Here the else arm is a statement expression that's supposed
         to be suppressed.  The label inside the while would unsuppress
	 code generation again if not handled correctly.  And that
	 would wreak havoc to the cond-expression because there's no
	 jump-around emitted, the whole statement expression really
	 needs to not generate code (perhaps except useless forward jumps).  */
      (1 ? 
       fprintf(stderr,"timeout=%ld\n", timeout) :
       ({
	int i = 1;
	while (1)
	  while (i--)
	    some_label:
	      fprintf(stderr,"error\n");
	goto some_label;
	})
      );
      timeout--;
  } while (timeout);
}

static int global;

static void foo(int i)
{
  global+=i;
  fprintf (stderr,"g=%d\n", global);
}

static int check(void)
{
  fprintf (stderr,"check %d\n", global);
  return 1;
}

static void dowhile(void)
{
  do {
      foo(1);
      if (global == 1) {
	  continue;
      } else if (global == 2) {
	  continue;
      }
      /* The following break shouldn't disable the check() call,
         as it's reachable by the continues above.  */
      break;
  } while (check());
}

int main (void)
{
  int i = 1;
  kb_wait_1();

  /* Simple test of dead code at first sight which isn't actually dead. */
  if (0) {
yeah:
      fprintf (stderr,"yeah\n");
  } else {
      fprintf (stderr,"boo\n");
  }
  if (i--)
    goto yeah;

  /* Some more non-obvious uses where the problems are loops, so that even
     the first loop statements aren't actually dead.  */
  i = 1;
  if (0) {
      while (i--) {
	  fprintf (stderr,"once\n");
enterloop:
	  fprintf (stderr,"twice\n");
      }
  }
  if (i >= 0)
    goto enterloop;

  /* The same with statement expressions.  One might be tempted to
     handle them specially by counting if inside statement exprs and
     not unsuppressing code at loops at all then.
     See kb_wait_1 for the other side of the medal where that wouldn't work.  */
  i = ({
      int j = 1;
      if (0) {
	  while (j--) {
	      fprintf (stderr,"SEonce\n");
    enterexprloop:
	      fprintf (stderr,"SEtwice\n");
	  }
      }
      if (j >= 0)
	goto enterexprloop;
      j; });

  /* The other two loop forms: */
  i = 1;
  if (0) {
      for (i = 1; i--;) {
	  fprintf (stderr,"once2\n");
enterloop2:
	  fprintf (stderr,"twice2\n");
      }
  }
  if (i > 0)
    goto enterloop2;

  i = 1;
  if (0) {
      do {
	  fprintf (stderr,"once3\n");
enterloop3:
	  fprintf (stderr,"twice3\n");
      } while (i--);
  }
  if (i > 0)
    goto enterloop3;

  /* And check that case and default labels have the same effect
     of disabling code suppression.  */
  i = 41;
  switch (i) {
      if (0) {
	  fprintf (stderr,"error\n");
      case 42:
	  fprintf (stderr,"error2\n");
      case 41:
	  fprintf (stderr,"caseok\n");
      }
  }

  i = 41;
  switch (i) {
      if (0) {
	  fprintf (stderr,"error3\n");
      default:
	  fprintf (stderr,"caseok2\n");
	  break;
      case 42:
	  fprintf (stderr,"error4\n");
      }
  }

  dowhile();

  return 0;
}
