#include <tasks/thunderdome.h>
#include <tasks/term_server.h>

typedef struct thunderdome_context {

    char name[32];


} context;


void thunderdome() {
    context ctxt;

    sprintf_string(ctxt.name, COLOUR(YELLOW) "Thunderdome" COLOUR_RESET);

    log("[%s] Two trains enter, one train leaves", ctxt.name);
}
