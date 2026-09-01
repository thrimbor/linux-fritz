/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
#ifndef __linux_pcmlink_ul_h__
#define __linux_pcmlink_ul_h__

#include <linux/types.h> 
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
enum _logic_chan_ll {
   logic_chan_isdn = 0
};

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum _logic_chan_ul {
	logic_chan_controlchan     = 0,       /*--- wird vom ul-proto (zuerst) aufgebaut: dient zum Lifettime-check, Debug, ul_release ... ---*/ 
    logic_chan_Messages        = 1,
	logic_chan_D_Kanal_TE      = 2,
	logic_chan_D_Kanal_NT      = 3,
	logic_chan_AnalogPOTS      = 4, /*---- pots ---*/    
	logic_chan_AnalogAB1       = 5, /*---- slic1 ---*/
	logic_chan_AnalogAB2       = 6, /*---- slic2 ---*/
	logic_chan_AnalogAB3       = 7, /*---- slic3 ---*/
	logic_chan_B1_Kanal_TE     = 8,/*--- isdn-te1 ---*/
	logic_chan_B2_Kanal_TE     = 9,/*--- isdn-te2 ---*/
	logic_chan_B1_Kanal_NT     = 10,/*--- isdn-nt1 ---*/
	logic_chan_B2_Kanal_NT     = 11,/*--- isdn-nt2 ---*/
	logic_chan_DECT_Kanal_1    = 12,/*--- dect ---*/
	logic_chan_DECT_Kanal_2    = 13,/*--- dect ---*/
	logic_chan_DECT_Kanal_3    = 14,/*--- dect ---*/
	logic_chan_DECT_Kanal_4    = 15,/*--- dect ---*/
    logic_chan_MaxChan         = 16 
};
#define PCMLINK_FIRST_DYNHW_CHAN   32  
#define PCMLINK_MAX_DYNHW_CHAN      3
/*-------------------------------------------------------------------------------------*\
\*-------------------------------------------------------------------------------------*/
enum _chan_message_voip_config{
    voip_chanx_off           = 0,
    voip_chanx_8khz          = 1,
    voip_chanx_8khz_only_rcv = 2
};

/*-------------------------------------------------------------------------------------*\
 * derzeitiges Format der Daten im logic_chan_Messages
\*-------------------------------------------------------------------------------------*/
struct _chan_message_voip {
    enum _logic_chan_ul ManhattanChan     :  5;
    enum _chan_message_voip_config Config :  2;
    unsigned int EchoCanceler             :  1;
    unsigned int Chanx_16khz              :  1; /*--- statt 8 kHz 16 kHz ---*/
    unsigned int Reserved                 : 23;
};
/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _chan_message_dsptomips {
    unsigned int TriggerCount;
    unsigned int DSP_XDU;
    unsigned int DSP_OVR;
    unsigned int MIPS_OVR;
    unsigned int ulRxMask;
    /*--- variable Anzahl _chan_dtmf; ---*/
};

/*--------------------------------------------------------------------------------*\
\*--------------------------------------------------------------------------------*/
struct _chan_dtmf {
#ifdef __BIG_ENDIAN
    enum _logic_chan_ul ManhattanChan:5;
    unsigned char Next:3;               /*--- 1 = es existiert Nachfolge-Element ---*/
#else/*--- #ifdef __BIG_ENDIAN ---*/
    unsigned char Next:3;               /*--- 1 = es existiert Nachfolge-Element ---*/
    enum _logic_chan_ul ManhattanChan:5;
#endif/*--- #else ---*//*--- #ifdef __BIG_ENDIAN ---*/
    unsigned char DTMF_Sign;
};

/*-------------------------------------------------------------------------------------*\
 * als Alternative zu _chan_message_dsptomips (Config-Info beim Startup)
 * (identisch mit der Struktur im fpgmemshared aber eben exportiert)
\*-------------------------------------------------------------------------------------*/
struct _chan_configmsg_dsptomips {
    unsigned int SupportFlag;
    unsigned int svnVersion;       
    unsigned int Reserved1;
    unsigned int Reserved2;
    unsigned int Flag;         /*--- muss -1 sein, zur Erkennung, dass keine struct _chan_message_dsptomips ---*/
};
#define PCMBUS_FPGA_DECTSLIC        0x1

#define PCMBUS_VERSION(a)       ((((a)->SupportFlag) >> 29) & 0x07)
#define PCMBUS_DTMF_SUPPORT(a)  ((((a)->SupportFlag) >> 28) & 0x01)
#define PCMBUS_MAX_AUDIOCHAN(a) ((((a)->SupportFlag) >> 26) & 0x03)
#define PCMBUS_MAX_ECCHAN(a)    ((((a)->SupportFlag) >> 22) & 0x0F)
#define PCMBUS_MAX_CODECCHAN(a) ((((a)->SupportFlag) >> 17) & 0x1F)
#define PCMBUS_MAX_TECHAN(a)    ((((a)->SupportFlag) >> 13) & 0x0F)
#define PCMBUS_MAX_NTCHAN(a)    ((((a)->SupportFlag) >>  9) & 0x0F)
#define PCMBUS_MAX_DECTCHAN(a)  ((((a)->SupportFlag) >>  5) & 0x0F)
#define PCMBUS_MAX_SLICCHAN(a)  ((((a)->SupportFlag) >>  2) & 0x07)
#define PCMBUS_MAX_POTSCHAN(a)  ((((a)->SupportFlag) >>  1) & 0x01)
#define PCMBUS_UK0(a)           ((((a)->SupportFlag) >>  0) & 0x01)

#if defined(__KERNEL__)
/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
enum _pcmlink_tasklet_control {
    pcmlink_tasklet_control_enter_critical = 0,
    pcmlink_tasklet_control_leave_critical = 1
};

/*--------------------------------------------------------------------------------*\
 * Race-Conditionschutz fuer alle die ueber Callback etc. mit dem PCMRouter verbunden sind
 * (capi_oslib, isdn-Treiber, DECT-Treiber)
\*--------------------------------------------------------------------------------*/
void pcmlink_tasklet_control(enum _pcmlink_tasklet_control);
/*--------------------------------------------------------------------------------*\
 * auf SMP-Systemen: pcmlink_tasklet_control funktioniert nur, wenn folgende CPU verwendet wird:
\*--------------------------------------------------------------------------------*/
#if defined(CONFIG_SMP)
#define PCMLINK_TASKLET_CONTROL_CPU     1
#else/*--- #if defined(CONFIG_SMP) ---*/
#define PCMLINK_TASKLET_CONTROL_CPU     0
#endif/*--- #else ---*//*--- #if defined(CONFIG_SMP) ---*/


/*--------------------------------------------------------------------------------*\
 * nur fuer ISDN-Treiber: Schreibt ein (TDM-)Register
\*--------------------------------------------------------------------------------*/
void pcmlink_ul_writeregister(unsigned int addr, unsigned int value, unsigned int mask);

/*--------------------------------------------------------------------------------*\
 * nur fuer ISDN-Treiber: liest ein (TDM-)Register
\*--------------------------------------------------------------------------------*/
unsigned int pcmlink_ul_readregister(unsigned int addr);

/*--------------------------------------------------------------------------------*\
 * nur fuer ISDN-Treiber: PCM-Router-Modell anmelden, das v3 Modell ist dann obsolet 
\*--------------------------------------------------------------------------------*/
void *pcmlink_ul_register_pcmrouter(void (*pcmrouter)(unsigned char *dsptomips, unsigned int dsptomipslen));

/*--------------------------------------------------------------------------------*\
 * nur fuer ISDN-Treiber: PCM-Router-Modell abmelden
\*--------------------------------------------------------------------------------*/
void pcmlink_ul_release_pcmrouter(void *handle);

/*--------------------------------------------------------------------------------*\
 * nur fuer ISDN-Treiber: PCM-Router-Modell - NUR aus PCM-Trigger-Kontext bedienen
\*--------------------------------------------------------------------------------*/
unsigned short pcmlink_ul_rxget_slotdata_pcmrouter(unsigned char slot_idx, unsigned int ul_chan, unsigned char **buf);

/*--------------------------------------------------------------------------------*\
 * nur fuer ISDN-Treiber: PCM-Router-Modell - NUR aus PCM-Trigger-Kontext bedienen
\*--------------------------------------------------------------------------------*/
void pcmlink_ul_rxfree_slotdata_pcmrouter(unsigned char slot_idx, unsigned int ul_chan);

/*--------------------------------------------------------------------------------*\
 * nur fuer ISDN-Treiber: PCM-Router-Modell - NUR aus PCM-Trigger-Kontext bedienen
\*--------------------------------------------------------------------------------*/
unsigned char *pcmlink_ul_txalloc_slotdata_pcmrouter(unsigned char slot_idx, unsigned int ul_chan);

/*--------------------------------------------------------------------------------*\
 * nur fuer ISDN-Treiber: PCM-Router-Modell - NUR aus PCM-Trigger-Kontext bedienen
\*--------------------------------------------------------------------------------*/
void pcmlink_ul_txput_slotdata_pcmrouter(unsigned char slot_idx, unsigned int ul_chan, unsigned short len);

/*--------------------------------------------------------------------------------*\
 * nur fuer ISDN-Treiber: PCM-Router-Modell - es empfiehlt sich dies aus PCM-Trigger-Kontext zu bedienen
\*--------------------------------------------------------------------------------*/
void pcmlink_ul_setslotparam_pcmrouter(unsigned char slot_idx, unsigned int RefChan, struct _chan_message_voip *VoipConfig);

/*--------------------------------------------------------------------------------*\
 * nur fuer ISDN-Treiber: PCMRouter-Modell 
\*--------------------------------------------------------------------------------*/
void pcmlink_ul_dspfwversion_pcmrouter(struct _chan_configmsg_dsptomips **fwversion);

/*--------------------------------------------------------------------------------*\
 * nur fuer dect-Treiber:
 * maxslotlen: liefert maximale Slotlaenge
 * p[maxdectslots] : Pointertabelle: hier Daten hineinschreiben/lesen - Pointer nicht modifizieren!
 * Achtung es muessen immer auch die (Dummy-)Daten fuer Tx komplett geschrieben werden!
 * Returnwert: Handle
\*--------------------------------------------------------------------------------*/
void *pcmlink_ul_register_dectcontrol(unsigned int *maxslotlen, unsigned int *maxdectslots,
                                      void RxTxCallback(unsigned char **p_rx, unsigned char **p_tx));

/*--------------------------------------------------------------------------------*\
 * nur fuer dect-Treiber:
\*--------------------------------------------------------------------------------*/
void pcmlink_ul_release_dectcontrol(void *handle);

/*--------------------------------------------------------------------------------*\
 * Oeffnet definierte Slots
 * linkhandle:  (fuer dect_audio transparente) Struktur, in der die zu verwendenden Slots enthalten sind
 * ref:          Referenz fuer Aufrufer 
 * RxTxCallback: Callback fuer die Slotdaten. Der Aufruf erfolgt direkt aus dem Irq-Kontext
 *               im 8 msec Takt. Achtung! Irq-Kontext bedeutet eingeschraenkter Funktionsumfang 
 *               und spezielle dynamische Speichernutzung (GFP_ATOMIC)
 *  transp_flag: 0: Samples 16 KHz (wird in G722 konvertiert), 1 : transp
 *   Parameter RxTxCallback:
 *              ref:          oben angebene Referenz 
 *              slotentries:  Anzahl der Slots (prxslottab[slotentries]/ptxslottab[slotentries])
 *              prxslottab[]: Slotzeigertabelle fuer vom PCM-Bus empfangene Daten
 *                            im Aufruf muessen die Daten ausgewertet/umkopiert werden
 *              ptxslottab[]: Slotzeigertabelle fuer zum PCM-Bus zu verschickende Daten - 
 *                            im Aufruf zu fuellen 
 *              slotlen:      Laenge der Daten pro Slot in Byte
 *
 *  Returnparameter: Slothandle            
 *              NULL: falls Slots bereits belegt/Konflikte mit vergebenen Slots/der Isdn-Treiber nicht laeuft
\*--------------------------------------------------------------------------------*/
void *pcmlink_ul_openslots(void *linkhandle, void *ref, 
                           void (*RxTxCallback)(void *ref, short slotentries, unsigned char *prxslottab[], unsigned char *ptxslottab[], size_t slotlen),
                           unsigned int transp_flag
                          );

/*--------------------------------------------------------------------------------*\
 * Schliesst aktuelle Slotverbindung
 * slothandle: Handle von pcmlink_ul_openslots()
\*--------------------------------------------------------------------------------*/
void pcmlink_ul_closeslots(void *slothandle);

/*--------------------------------------------------------------------------------*\
 * Anhand des linkhandles wird ein evtl. schon existierendes Open-Handle 
 * ermittelt. Falls nicht wird NULL zurueckgeliefert
\*--------------------------------------------------------------------------------*/
void *pcmlink_ul_linkhandle_to_handle(void *linkhandle);

/*--------------------------------------------------------------------------------*\
 * nur fuer ISDN-Treiber: Notifikation ab welchen DECT-Channel VOICE-Daten moeglich sind
 * MaxDECTCchannel: maximale Anzahl der DECT-Kanaele fuer VOICE
\*--------------------------------------------------------------------------------*/
void *pcmlink_ul_register_dectcontrolnotification(void notification(unsigned int FirstDECTChannel, unsigned int MaxDECTCchannel));

/*--------------------------------------------------------------------------------*\
 * nur fuer ISDN-Treiber: Notification abmelden
\*--------------------------------------------------------------------------------*/
void pcmlink_ul_unregister_dectcontrolnotification(void *handle);

/*--------------------------------------------------------------------------------*\
 * nur fuer dect-Treiber: rxmask/txmask: die in mask angegebenen Bits werden als dect-control geroutet
\*--------------------------------------------------------------------------------*/
void pcmlink_ul_config_dectcontrol(void *handle, enum  _logic_chan_ul rxmask, enum _logic_chan_ul txmask);

#define PCMLINK_ACTIVITY_RESERVED_BITS    (32)
#define PCMLINK_ACTIVITY_DTE     ((unsigned long long)1 << (32 + 0))
#define PCMLINK_ACTIVITY_DNT     ((unsigned long long)1 << (32 + 1))
#define PCMLINK_ACTIVITY_POTS    ((unsigned long long)1 << (32 + 2))
#define PCMLINK_ACTIVITY_DECT    ((unsigned long long)1 << (32 + 3))  /*--- fuer avm_dect-Treiber reserviert ---*/
#define PCMLINK_ACTIVITY_SLIC(a) ((unsigned long long)1 << (32 + 3 +(a))) /*--- 1 - 3 ! ---*/
/*--------------------------------------------------------------------------------*\
 * Ausschalten des FS/Timers bei Nichtaktivitaet
 * unteren 32 Bit sind fuer BCHAN (also logische Kanaele) reserviert
 * set: 0 oder 1
\*--------------------------------------------------------------------------------*/
int pcmlink_ul_activity(long long mask, int set);

/*--------------------------------------------------------------------------------*\
   Meldet dynamischen HwChannel-Support (z.B. Mini) am PCM-Router an
   maxslotlen: liefert die maximale Groesse des Buffers pro Callback in Bytes (Wideband 16 Bit-linear)
   maxslots:   liefert maximal gleichzeitig anmeldbare HwChannels
   idname:      Identifikator (Mini: "MINI")
   RxTxCallback(): Callback: wird mit festen 8 msec Trigger bei aktivierter Verbindung regelmaessig aufgerufen 
                   channel_id:  die mit fonctrl ausgehandelte Channel-ID - Achtung - durch Ab/Aufbauverzoegerung 
                   durchaus auch nicht zugeordnet (DynHwChan muss Callbackaufruf entsprechend ignorieren)!
                   *p_txrx:  Pointer auf Daten zum DynHwChan -> muss umgesetzt werden auf Pointer mit Daten fuer Router
                             Daten muessen bis zum naechsten Aufruf der RxTxCallback() gueltig bleiben!
                   *slotlen: Laenge zum DynHwChan (Byte) -> gleichzeitig "Loadbalance"-Wert: soviel Daten erwartet der Router.
                             Muss umgesetzt werden auf Laenge der zum Router zu sendenden Daten.
                             Falls DynHwChan nicht soviel Daten liefern kann, so kann er auch weniger Daten liefern, aber nie mehr!
                             (PLC des Routers schlaegt zu)
                   Kontext:  Achtung! Interrupt/Tasklet-Kontext: Nur sehr eingeschraenkte Verwendung von Kernelfunktionen
                             in der Callback erlaubt
    Returnwert: Handle                           
\*--------------------------------------------------------------------------------*/
void *pcmlink_ul_register_dynhwchannel_control(unsigned short *maxslotlen, unsigned short *maxslots, const char *idname, 
                                       void RxTxCallback(unsigned int channel_id, unsigned char **p_txrx, unsigned short *slotlen));

/*--------------------------------------------------------------------------------*\
 * Meldet HwChannel-Support am PCM-Router ab
\*--------------------------------------------------------------------------------*/
void pcmlink_ul_release_dynhwchannel_control(void *handle);
#endif/*--- #if defined(__KERNEL__) ---*/
#endif/*--- #ifndef __linux_pcmlink_ul_h__ ---*/
