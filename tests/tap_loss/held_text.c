/* tap-held: the text service as built (this copy replaces the library's), plus two ways its candidate list changes
 * without the person while a finger is on the keyboard (tests/tap_loss/held.c). */
#include "../../system/services/text/text_service.c"

/* A continuation arrives the way a hub reply does: through assist_show on the UI task, for this input and revision. */
void tap_held_suggest(const char *const *words, int n) {
    assist_reply_t *r = osal_calloc(1, sizeof *r, OSAL_MEM_EXTERNAL);
    if (!r) return;
    r->generation = AS.generation; r->revision = TS.revision; r->session = TS.session;
    r->epoch = ink_hub_member_epoch(); r->connection = ink_hub_connection();
    snprintf(r->member, sizeof r->member, "%s", TS.owner.member);
    for (int i = 0; i < n && i < 3; i++) {
        snprintf(r->words[i].word, sizeof r->words[i].word, "%s", words[i]);
        r->words[i].ai = true;
        r->n++;
    }
    assist_show(r);
}

/* The pinyin list reorders and the keyboard is told, as when the dictionary comes in while letters are composed
 * (on_dict_ready). The real load cannot be timed into a press, so its effect is made here: the first two swap. */
void tap_held_reorder(void) {
    compute_cands();
    if (TS.ncands < 2) return;
    ink_text_cand_t first = TS.cands[0];
    TS.cands[0] = TS.cands[1];
    TS.cands[1] = first;
    if (TS.session && TS.presenter) TS.presenter();
}
