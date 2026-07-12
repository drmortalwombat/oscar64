#include "ambiguousoverload.h"

/**
 * Regression test for function pointers in aggregate initialisers.
 *
 * This setup function must be in a different translation unit from clearView's definition and
 * direct call (ambiguousoverload_trigger.c). The bug reused clearView's function declaration as an
 * aggregate-initialiser list node and corrupted its overload chain. Parsing the definition in the
 * other translation unit then made the otherwise unambiguous direct call fail.
 *
 * Holder reproduces the original ambiguous-overload diagnostic. The single-member CallbackHolder
 * also covers the related initialiser-builder assertion. Both callbacks are invoked to verify that
 * their aggregate initialisers contain the correct function address. The pragma in
 * ambiguousoverload.h schedules the companion translation unit so this remains one autotest target.
 */
void setup(void)
{
	struct Holder holder = { .value = 1, .callback = clearView };
	struct CallbackHolder callbackHolder = { .callback = clearView };
	holder.callback();
	callbackHolder.callback();
}
