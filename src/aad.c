#include <security/pam_appl.h>
#include <security/pam_modules.h>

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdarg.h>
#include <string.h>

#define PAM_SM_AUTH

#ifndef PAM_EXTERN
#define PAM_EXTERN
#endif

#if !defined(LOG_AUTHPRIV) && defined(LOG_AUTH)
#define LOG_AUTHPRIV LOG_AUTH
#endif

#ifdef sun 
#define PAM_CONST
#else
#define PAM_CONST const 
#endif

#define MODULE_NAME "pam_azure_authenticator"
#define CODE_PROMPT "Enter the following code at https://aka.ms/devicelogin : "

typedef struct Params {
    int         echocode;
    int         allowed_perm;
    int         debug;
} Params;

static void log_message(int priority, pam_handle_t *pamh,
                        const char *format, ...) {
  char logname[80];
  snprintf(logname, sizeof(logname), "%s(" MODULE_NAME ")");

  va_list args;
  va_start(args, format);
#if !defined(DEMO) && !defined(TESTING)
  openlog(logname, LOG_CONS | LOG_PID, LOG_AUTHPRIV);
  vsyslog(priority, format, args);
  closelog();
#else
  if (!error_msg) {
    error_msg = strdup("");
  }
  {
    char buf[1000];
    vsnprintf(buf, sizeof buf, format, args);
    const int newlen = strlen(error_msg) + 1 + strlen(buf) + 1;
    char* n = malloc(newlen);
    if (n) {
      snprintf(n, newlen, "%s%s%s", error_msg, strlen(error_msg)?"\n":"",buf);
      free(error_msg);
      error_msg = n;
    } else {
      fprintf(stderr, "Failed to malloc %d bytes for log data.\n", newlen);
    }
  }
#endif

  va_end(args);
}

static int converse(pam_handle_t *pamh, int nargs, 
                  PAM_CONST struct pam_message **message,
                  struct pam_response **response){
    struct pam_conv *conv;
    int retval = pam_get_item(pamh, PAM_CONV, (void *)&conv);
    if (retval != PAM_SUCCESS){
        return retval;
    }
    return conv->conv(nargs, message, response, conv->appdata_ptr);
}

static const char *get_user_name(pam_handle_t *pamh, const Params *params){
    //Obtain user's name 
    const char *username;
    if (pam_get_user(pamh, &username, NULL) != PAM_SUCCESS || 
      !username || !*username){
          log_message(LOG_ERR, pamh,
          "pam_get_user() failed to get a user name");

          return NULL;
      }
      if (params -> debug){
          log_message(LOG_INFO, pamh, "debug: start of azure_authenticator for %s", username);
      }
      return username;
}

static char *request_code(){
    return "GHSDJDFDJD";
}

static char *request_pass(pam_handle_t *pamh, int echocode){
  char prompt[100], code[100];
  strcpy(prompt, CODE_PROMPT);
  strcpy(code, request_code());
  strcat(prompt, code);
  PAM_CONST char *message = prompt;
  PAM_CONST struct pam_message msg = { .msg_style = echocode,
                                        .msg = message };
  PAM_CONST struct pam_message *msgs = &msg;
  struct pam_response *resp = NULL;
  int retval = converse(pamh, 1, &msgs, &resp);
  char *ret = NULL;
  if (retval != PAM_SUCCESS || resp == NULL || resp->resp == NULL || 
    *resp->resp == '\000'){
        log_message(LOG_ERR, pamh, "Did not recieve verification code from user");
        if (retval == PAM_SUCCESS && resp && resp->resp){
            ret = resp->resp;
        }
    }else{
        ret = resp->resp;
    }
    if (resp){
        if (!ret){
            free(resp->resp);
        }
        free(resp);
    }
    return ret;
  }

static int parse_args(pam_handle_t *pamh, int argc, const char **argv,
                     Params *params){
params->debug = 0;
params->echocode = PAM_PROMPT_ECHO_OFF;
int i;
for (i = 0; i < argc; ++i){
    printf("ok");
}
return 0;
}            

static int azure_authenticator(pam_handle_t *pamh, int flags,
                               int argc, const char **argv){

  int rc = PAM_AUTH_ERR;
  const char *username;
  const char *pw;

  Params params = { 0 };
  params.allowed_perm = 0600;
  if (parse_args(pamh, argc, argv, &params) < 0){
      return rc;
  }

  username = get_user_name(pamh, &params);
  log_message(LOG_INFO, pamh, "debug: Collected username for user %s", username);

  pw = request_pass(pamh, params.echocode);
  return PAM_SUCCESS;
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags,
                                   int argc, const char **argv){
  return azure_authenticator(pamh, flags, argc, argv);                                
}

PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc,
                                    const char **argv){
  return PAM_SUCCESS;
}
