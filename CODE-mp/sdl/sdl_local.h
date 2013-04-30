void IN_Init( void );
void IN_Shutdown( void );
void IN_Restart( void );

void Sys_GLimpSafeInit( void );
void Sys_GLimpInit( void );
void Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr );
