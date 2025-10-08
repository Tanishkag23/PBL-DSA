// auth.h
#ifndef AUTH_H
#define AUTH_H

/* handles signup/login; returns logged-in username string (caller must not free)
   returns NULL on failure/exit */
char* auth_prompt(); 

#endif
