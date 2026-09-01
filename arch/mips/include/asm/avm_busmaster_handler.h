/******************************************************************************
** FILE NAME    : avm_busmaster_handler.h
** COMPANY      : AVM
** AUTHOR       : Heiko Blobner
** DESCRIPTION  : Unter anderem bei der Umschaltung des Taktes bei der z.B. 7320
**                ist es noetig, alle RAM-Zugriffe zu unterbinden.
**                Dieses Modul stellt einen Mechanismus zur Verfuegung, der
**                registrierte Busmaster stoppen und wieder laufenlassen kann.
**
**                Ablauf:
**                -'prepare_stop'
**                  Aufgabe dieses Aufrufes ist es, dass die Devices sich auf die
**                  Unterbrechung des RAM Zugriffs vorbereiten können (z.B. keine
**                  neuen Aktion, DMAs aufsetzen)
**
**                  Return-Werte:
**                   .0      ..keine Fehler
**                   .EBUSY  ..beschaeftigt, sofort nocheinmal probieren (max 2mal)
**                   .EAGAIN ..bereite Umschalten vor, nach  'schedule()' nochmal triggern
**                   .'else' ..Fehler (Vorgang abbrechen)
**
**                -'stop'
**                  RAM-Zugriffe werden gestoppt.
**
**                  Return-Werte:
**                   .0      ..keine Fehler
**                   .EBUSY  ..beschaeftigt, sofort nocheinmal probieren (max 2mal)
**                   .'else' ..Fehler (Vorgang abbrechen)
**
**                -'run'
**                  Auf das RAM kann wieder zugegrieffen werden.
**
**                  Return-Werte:
**                   .0      ..keine Fehler
**                   .'else' ..Fehler (Vorgang abbrechen)
**
*******************************************************************************/
#ifndef AVM_BUSMASTER_HANDLER__H
#define AVM_BUSMASTER_HANDLER__H

//-Types---------
enum _avm_busmaster_cmd {
	avm_busmaster_prepare_stop,
	avm_busmaster_stop,
	avm_busmaster_run
};

//-Functions-----
void avm_register_busmaster    (char *name, int (*callback)(enum _avm_busmaster_cmd cmd));
void avm_release_busmaster     (char *name);
int  avm_run_busmaster         (void);
int  avm_prepare_busmaster_stop(void);
int  avm_stop_busmaster        (void);

#endif /*--- #define AVM_BUSMASTER_HANDLER__H ---*/

