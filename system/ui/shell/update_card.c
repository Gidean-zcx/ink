/* Keep the boot entry; update presenters live together. */
int ink_update_pages_init(void);
int ink_shell_update_card_init(void) { return ink_update_pages_init(); }
