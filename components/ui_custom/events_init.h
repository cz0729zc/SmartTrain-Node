/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/


#ifndef EVENTS_INIT_H_
#define EVENTS_INIT_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "gui_guider.h"

void events_init(lv_ui *ui);

typedef enum {
	EVENTS_SC_RFID = 0,
	EVENTS_SC_FACE,
	EVENTS_SC_FINGER,
	EVENTS_SC_NETWORK,
} events_selfcheck_item_t;

void events_selfcheck_set_result(events_selfcheck_item_t item, bool ok, const char *log_text);
void events_selfcheck_finish(void);

void events_show_standby(void);
void events_standby_update_status(const char *time_text, bool wifi_connected);
void events_show_admin(const char *card_text);
void events_show_confirm(const char *student_id, const char *card_id);
void events_show_unregistered(const char *card_id, const char *reason);

#ifdef __cplusplus
}
#endif
#endif /* EVENT_CB_H_ */
