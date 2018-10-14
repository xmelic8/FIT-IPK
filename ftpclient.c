/* Autor: Michal Melichar                                                    *
 * Login: xmelic17                                                           *
 * E-Mail: xmelic17@stud.fit.vutbr.cz                                        *
 * Funkce programu: Program s vyuzitim rozhrani stranek (sockets API) vypise *
 * na standartni vystup obsah vzdaleneho adresaze FTP severu s vyuzitim      *
 * protokolu FPT.                                                            *
 * Parametry prgramu: URL adresa FTP severu ve tvaru =>                      *
 *                    [ftp://[user:password@]]host[:port][/path][/]          *
 * Navratova hodnota programu: -1 => pri chybe                               *
 *                              0 => pokud program probehne v poradku        */


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdarg.h>
#include <math.h>
#include <sys/ioctl.h>
#include <stdbool.h>


#define BUFFER_SIZE 256 // Pocatecni velikost bufferu


/* Globalni struktura obsahujici promenne reprezentujici format URL zadany *
 * parametrem.                                                             */
typedef struct{
char *typ_spojeni;
char *password;
char *user;
char *host;
char *port;
char *path;
int velikost_user;
int velikost_password;
int velikost_path;
} URL;


// Vyuctovy typ obsahujici jednotlive chyby
enum{
  ERR_PARAMETR,
  ERR_PORT,
  ERR_ALOK_MEM,
  ERR_PROTOKOL,
  ERR_PROTOKOL2,
  ERR_SOCKET,
  ERR_PREKLAD,
  ERR_PRIPOJENI,
  ERR_KONEC_PRIPOJENI,
  ERR_KOMUNIKACE,
};


// Pole obsahujici jednotlive chybove hlasky
const char *ERROR[] = {
  "Spatne zadany parametr",
  "Chybne zadane cislo portu",
  "Chyba pri alokaci pameti",
  "Nepodporovany typ protokolu",
  "Zadano user a password, nebyl zadan protokol",
  "Nepodarilo se vytvorit socket",
  "Nepodarilo se prelozit adresu",
  "Doslo k chybe pri pokusu o pripojeni k severu",
  "Doslo k chybe pri uzavirani spojeni mezi klientem a serverem",
  "Doslo k chybe pri komunikace se serverem",
};


// Hlavicky funkci
void print_err(int error);
int zpracuj_parametr(char *argv, URL *url_parametry);
void uvolni_pamet(URL *url_parametry);



int main(int argc, char **argv){
  char *buffer_ridici = NULL; // Ukazatel na buffer pro ridici spojeni
  char *buffer_datovy = NULL; // Ukazatel na buffer pro datove spojeni
  char *pomocny_buffer = NULL;
  int vytvoreny_socket; // Identifikator socketu na ridicim spojeni
  int vytvoreny_dat_socket; // Identifikator socketu na datovem spojeni
  unsigned int cislo_portu = 21;
  unsigned int velikost_pozadavku = 0;


  if(argc != 2){
    print_err(ERR_PARAMETR);
  }

  //Inicialiazace prvku struktury
  URL parametry = {NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0};

  int navrat_hodnota_fce = zpracuj_parametr(argv[1], &parametry);
  if(navrat_hodnota_fce != 0){
    switch(navrat_hodnota_fce){
      case 1:
        print_err(ERR_ALOK_MEM);
        break;
      case 2:
        print_err(ERR_PORT);
        break;
      case 3:
        print_err(ERR_PROTOKOL2);
        break;
    }
    uvolni_pamet(&parametry);
    return -1;
  }

  //Osetreni pokud bude zadan jiny protokol nez FTP
  if((parametry.typ_spojeni != NULL) && (strcmp(parametry.typ_spojeni, "ftp://") != 0)){
    print_err(ERR_PROTOKOL);
    uvolni_pamet(&parametry);
    return -1;
  }

  //Pokud bylo zadano cislo portu, prevede se z retezce na cislo
  if(parametry.port != NULL)
    cislo_portu = atoi(parametry.port);

  //Pokud nebylo zadano prihlasovaci jmeno a heslo
  if((parametry.user == NULL) && (parametry.password == NULL)){
    parametry.velikost_password = strlen("password\r\n");
    parametry.velikost_user = strlen("anonymous\r\n");
    parametry.user = (char*) malloc(parametry.velikost_user * sizeof(char) + 1);
    parametry.password = (char*) malloc(parametry.velikost_password * sizeof(char) + 1);
    strcpy(parametry.user, "anonymous\r\n");
    strcpy(parametry.password, "password\r\n");
  }

  // Doplneni pokud nebyla zadana cesta k adresari
  if(parametry.path == NULL){
    parametry.path = (char *) malloc(4 * sizeof(char));
    strcpy(parametry.path, "/\r\n");
    parametry.velikost_path = 3;
  }


//-----------------------------------------------------
  // PO DOKONCENI SMAZAT!!!!
  //printf("%s\n", parametry.typ_spojeni);
  //printf("%s", parametry.user);
  //printf("%s", parametry.password);
  //printf("%s\n", parametry.host);
  //printf("%s\n", parametry.port);
  //printf("%s\n", parametry.path);
//---------------------------------------------------------

  struct sockaddr_in ftp_socket;
  struct hostent *preloz_adresa;
  ftp_socket.sin_family = PF_INET;
  ftp_socket.sin_port = htons(cislo_portu);

  //Vytvorim socket
  if((vytvoreny_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0){
    print_err(ERR_SOCKET);
    uvolni_pamet(&parametry);
    return -1;
  }

  //Prelozim adresu
  if((preloz_adresa = gethostbyname(parametry.host)) == NULL){
    print_err(ERR_PREKLAD);
    uvolni_pamet(&parametry);
    return -1;
  }

  memcpy(&(ftp_socket.sin_addr), preloz_adresa->h_addr_list[0], preloz_adresa->h_length);

  //Spojeni se serverem
  if(connect(vytvoreny_socket, (struct sockaddr *) &ftp_socket, sizeof(ftp_socket)) < 0){
    print_err(ERR_PRIPOJENI);
    uvolni_pamet(&parametry);
    return -1;
  }

  //Zpracovani pozadavku na server: prihlasovaci jmeno, heslo a  zvoleny rezim
  velikost_pozadavku = strlen("USER PASS EPSV\r\n") +
                       parametry.velikost_password + parametry.velikost_user;
  char *pozadavek_server = (char *) malloc(velikost_pozadavku * sizeof(char) + 1);
  pozadavek_server = strncpy(pozadavek_server, "USER ", 6);
  pozadavek_server = strcat(pozadavek_server, parametry.user);
  pozadavek_server = strcat(pozadavek_server, "PASS ");
  pozadavek_server = strcat(pozadavek_server, parametry.password);
  pozadavek_server = strcat(pozadavek_server, "EPSV\r\n");

  //-----------------------------------------------------
  // PO DOKONCENI SMAZAT!!!!
  //printf("%s", pozadavek_server);
  //-----------------------------------------------------

  // Odeslu pozadavek severu
  if(send(vytvoreny_socket, pozadavek_server, strlen(pozadavek_server), 0) == -1){
    print_err(ERR_KOMUNIKACE);
    free(pozadavek_server);
    pozadavek_server = NULL;
    uvolni_pamet(&parametry);
    return -1;
  }

  free(pozadavek_server);
  pozadavek_server = NULL;

  buffer_ridici = (char *) malloc((BUFFER_SIZE + 1)* sizeof(char));
  pomocny_buffer = (char *) malloc((BUFFER_SIZE + 1) * sizeof(char));

  sleep(1);
  //Promenna pro ulozeni poctu prectenych znaku funkci recv
  int pocet_prect_znaku = 0;
  int dontblock = 1;// Promenna pro nastaveni neblokovaciho rezimu
  char *p_p = NULL;// Pomocny ukazatel pri realokaci
  int i = 1;// Pomocna promenna urcujici mnozstvi dale alokovanych dat

  ioctl(vytvoreny_socket, FIONBIO, (char *) &dontblock);

  // Cteni z ridiciho spojeni, ocekavam cislo portun, na ktery se mam pripojit
  while((pocet_prect_znaku = recv(vytvoreny_socket, pomocny_buffer, BUFFER_SIZE, 0)) > 0){
    pomocny_buffer[BUFFER_SIZE] = '\0';
    if(i++ == 1)
      buffer_ridici = strncpy(buffer_ridici, pomocny_buffer, BUFFER_SIZE + 1);
    else{
      if(pocet_prect_znaku == BUFFER_SIZE){
        p_p = (char *) realloc(buffer_ridici ,((BUFFER_SIZE + 1) * i++) *  sizeof(char));
        if(p_p == NULL){
          print_err(ERR_ALOK_MEM);
          free(pomocny_buffer);
          pomocny_buffer = NULL;
          uvolni_pamet(&parametry);
          return -1;
        }
        buffer_ridici = p_p;
      }
      buffer_ridici = strncat(buffer_ridici, pomocny_buffer, BUFFER_SIZE + 1);
    }
    memset(pomocny_buffer, '\0', (BUFFER_SIZE + 1));
  }

  //-----------------------------------------------------
  // PO DOKONCENI SMAZAT!!!!
  //printf("%s", buffer_ridici);
  //-----------------------------------------------------

  free(pomocny_buffer);
  pomocny_buffer = NULL;

  if(strstr(buffer_ridici, "229 Entering Extended Passive Mode (|||") == NULL){
    print_err(ERR_KOMUNIKACE);
    uvolni_pamet(&parametry);
    return -1;
  }

  //Zpracovani portu, ktery mi poslal server
  char *oddelovac_socket = NULL;
  oddelovac_socket = strstr(buffer_ridici, "|||");
  oddelovac_socket += 3;
  int datovy_port = 0;
  while(*oddelovac_socket < '|'){
    datovy_port = ((datovy_port * 10) + (*oddelovac_socket) - '0');
    oddelovac_socket++;
  }

  ftp_socket.sin_port = htons(datovy_port);
  free(buffer_ridici);
  buffer_ridici = NULL;

  //Vytvoreni datoveho socketu
  if((vytvoreny_dat_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0){
    print_err(ERR_SOCKET);
    uvolni_pamet(&parametry);
    return -1;
  }

  //Spojeni se serverem pomoci datoveho socketu
  if(connect(vytvoreny_dat_socket, (struct sockaddr *) &ftp_socket, sizeof(ftp_socket)) < 0){
    print_err(ERR_PRIPOJENI);
    uvolni_pamet(&parametry);
    return -1;
  }

  velikost_pozadavku = strlen("LIST ") + parametry.velikost_path;
  pozadavek_server = (char *) malloc(velikost_pozadavku * sizeof(char) + 1);
  pozadavek_server = strncpy(pozadavek_server, "LIST ", 6);
  pozadavek_server = strcat(pozadavek_server, parametry.path);

  //Odeslani pozadavku na na port 21, vysledek ocekvana na portu ftp_socket.sin.port
  if(send(vytvoreny_socket, pozadavek_server, strlen(pozadavek_server), 0) == -1){
    print_err(ERR_KOMUNIKACE);
    free(pozadavek_server);
    pozadavek_server = NULL;
    uvolni_pamet(&parametry);
    return -1;
  }

  free(pozadavek_server);
  pozadavek_server = NULL;

  buffer_datovy = (char *) calloc((BUFFER_SIZE + 1), sizeof(char));

  // Cteni odpovedi severu na datovem socketu => obsah adresare
  while(recv(vytvoreny_dat_socket, buffer_datovy, BUFFER_SIZE, 0) > 0){
    buffer_datovy[BUFFER_SIZE] = '\0';
    fprintf(stdout, "%s", buffer_datovy);
    memset(buffer_datovy, '\0', BUFFER_SIZE + 1);
  }

  free(buffer_datovy);
  buffer_datovy = NULL;

  // Uzavreni datoveho spojeni mezi klientem a serverem
  if(close(vytvoreny_dat_socket) < 0){
    print_err(ERR_KONEC_PRIPOJENI);
    uvolni_pamet(&parametry);
    return -1;
  }

    // Uzavreni ridiciho spojeni mezi klientem a serverem
  if(close(vytvoreny_socket) < 0){
    print_err(ERR_KONEC_PRIPOJENI);
    uvolni_pamet(&parametry);
    return -1;
  }

  uvolni_pamet(&parametry);
  return 0;
}



// Implementace jednotlivych funkci
// Funkce pro vypis chybovych hlasenni
void print_err(int error){
  fprintf(stderr, "ERROR - %s\n", ERROR[error]);
}


// Funkce pro zpracovani parametru
int zpracuj_parametr(char *argv, URL *url_parametry){
  char *dve_lomitka = NULL;// Ukazatel na ulozeni pozice //
  char *zavinac = NULL;// Ukazatel na ulozeni pozice zavinace
  char *nalezeny_znak = NULL;// Ukazatel pro docasne nalezene znaky
  // Pomocny ukazatel, ktery se posouva v argv podle cteni
  char *pomocny_argv = argv;
  int delka_kopirovani;// Promenna urcujici delku kopirovani retezce
  // Promenna urcujici pokud byl zadan user a password musi byt zadan protokol
  bool protokol = false;


  //Podminka zpracuje zda byl zadan protokol
  if((dve_lomitka = strstr(argv, "//")) != NULL){
    delka_kopirovani = dve_lomitka - (&argv[0]) + 3;
    url_parametry->typ_spojeni = (char*) malloc(delka_kopirovani * sizeof(char));
    if(url_parametry->typ_spojeni == NULL)
      return 1;
    url_parametry->typ_spojeni = strncpy(url_parametry->typ_spojeni, pomocny_argv, delka_kopirovani);
    url_parametry->typ_spojeni[--delka_kopirovani] = '\0';
    pomocny_argv = argv + delka_kopirovani;
    protokol = true;
}


  //Podminka zpracuje, pokud byly zadany, prihlasovaci udaje
  if((zavinac = strchr(argv, '@')) != NULL){
    //Jsou-li zadane prihlasovaci udaje, je nutne aby byl zadan take protokol
    if(protokol != true){
      uvolni_pamet(url_parametry);
      return 3;
    }

    //Zpracovani uzivatelskeho jmena
    nalezeny_znak = strchr(pomocny_argv, ':');
    delka_kopirovani = nalezeny_znak - pomocny_argv;
    url_parametry->user = (char*) malloc((delka_kopirovani + 3) * sizeof(char));
    url_parametry->velikost_user = delka_kopirovani + 3;
    if(url_parametry->user == NULL){
      uvolni_pamet(url_parametry);
      return 1;
    }

    url_parametry->user = strncpy(url_parametry->user, pomocny_argv, delka_kopirovani);
    pomocny_argv = pomocny_argv + delka_kopirovani + 1;
    url_parametry->user[delka_kopirovani] = '\r';
    url_parametry->user[++delka_kopirovani] = '\n';
    url_parametry->user[++delka_kopirovani] = '\0';

    //Zpracovani hesla
    delka_kopirovani = zavinac - pomocny_argv;
    url_parametry->password = (char*) malloc((delka_kopirovani + 3) * sizeof(char));
    url_parametry->velikost_password = delka_kopirovani + 3;
    if(url_parametry->password == NULL){
      uvolni_pamet(url_parametry);
      return 1;
    }

    url_parametry->password = strncpy(url_parametry->password, pomocny_argv, delka_kopirovani);
    pomocny_argv = pomocny_argv + delka_kopirovani + 1;
    url_parametry->password[delka_kopirovani] = '\r';
    url_parametry->password[++delka_kopirovani] = '\n';
    url_parametry->password[++delka_kopirovani] = '\0';
  }


  //Nacitani parametru host
  if((nalezeny_znak = strchr(pomocny_argv, ':')) != NULL)
    delka_kopirovani = nalezeny_znak - pomocny_argv;
  else if((nalezeny_znak = strchr(pomocny_argv, '/')) != NULL)
    delka_kopirovani = nalezeny_znak - pomocny_argv;
  else{
    nalezeny_znak = strchr(pomocny_argv, '\0');
    delka_kopirovani = nalezeny_znak - pomocny_argv;
  }

  url_parametry->host = (char*) malloc((delka_kopirovani + 1) * sizeof(char));
  if(url_parametry->host == NULL){
    uvolni_pamet(url_parametry);
    return 1;
  }

  url_parametry->host = strncpy(url_parametry->host, pomocny_argv, delka_kopirovani);
  pomocny_argv = pomocny_argv + delka_kopirovani;
  url_parametry->host[delka_kopirovani] = '\0';


  //Otestovani zda nejsme na konci retezce
  if(*pomocny_argv == '\0')
    return 0;


  // Osetreni nacteni parametru port
  if(*pomocny_argv == ':'){//Byl zadan port, jdeme jej nacist
    if((nalezeny_znak = strchr(pomocny_argv, '/')) != NULL)
      delka_kopirovani = nalezeny_znak - (++pomocny_argv);
    else{
      nalezeny_znak = strchr(pomocny_argv, '\0');
      delka_kopirovani = nalezeny_znak - pomocny_argv;
    }

    url_parametry->port = (char*) malloc((delka_kopirovani + 1) * sizeof(char));
    if(url_parametry->port == NULL){
      uvolni_pamet(url_parametry);
      return 1;
    }

    url_parametry->port = strncpy(url_parametry->port, pomocny_argv, delka_kopirovani);
    pomocny_argv = pomocny_argv + delka_kopirovani;
    url_parametry->port[delka_kopirovani] = '\0';
  }

  //Cyklus nacitajici parametr path - adresarovou cestu
  nalezeny_znak = strchr(pomocny_argv, '\0');
  delka_kopirovani = nalezeny_znak - pomocny_argv;
  url_parametry->velikost_path = delka_kopirovani + 3;

  if(delka_kopirovani != 0){
    url_parametry->path = (char*) malloc((delka_kopirovani + 3) * sizeof(char));
    if(url_parametry->path == NULL){
      uvolni_pamet(url_parametry);
      return 1;
    }

    url_parametry->path = strncpy(url_parametry->path, pomocny_argv, delka_kopirovani);
    url_parametry->path[delka_kopirovani] = '\r';
    url_parametry->path[++delka_kopirovani] = '\n';
    url_parametry->path[++delka_kopirovani] = '\0';
  }

  return 0;
}


//Funkce pro uvolneni pameti.
void uvolni_pamet(URL *url_parametry){
 if(url_parametry->host != NULL){
    free(url_parametry->host);
    url_parametry->host = NULL;
  }

 if(url_parametry->password != NULL){
    free(url_parametry->password);
    url_parametry->password = NULL;
  }

  if(url_parametry->path != NULL){
    free(url_parametry->path);
    url_parametry->path = NULL;
  }

  if(url_parametry->typ_spojeni != NULL){
    free(url_parametry->typ_spojeni);
    url_parametry->typ_spojeni = NULL;
  }

  if(url_parametry->port != NULL){
    free(url_parametry->port);
    url_parametry->port = NULL;
  }

  if(url_parametry->user != NULL){
    free(url_parametry->user);
    url_parametry->user = NULL;
  }
}
