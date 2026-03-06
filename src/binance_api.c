#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <curl/curl.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"

char* BODYFILENAME = "body.out";
char* HEADERFILENAME = "head.out";
char* BASE_API_URL = "https://api.binance.com/api/v3";
char* CSV_FILENAME = "historical_data.csv";
char* ALLOWED_ASSET_ENV = ".allowed_assets";

int VERBOSE = 1;

//write to file function for curl
static size_t write_cb(void* ptr, size_t size, size_t nmemb, void* stream){
    size_t written = fwrite(ptr, size, nmemb, (FILE*)stream);
    return written;
}

//parse the contents of a file to a cJSON object
cJSON* parse_to_json(char* bfilename){
    if(bfilename == NULL){
        bfilename = BODYFILENAME;
    }

    FILE* f;
    f = fopen(bfilename, "r");

    //set to point to the end of the file
    fseek(f, 0, SEEK_END);

    //size bytes in file
    size_t size = ftell(f) + 1;

    fclose(f);

    f = fopen(bfilename, "r");

    //get line from file
    char* line = malloc(sizeof(char) * size);
    fgets(line, size, f);

    fclose(f);

    //parse to json
    cJSON* json = cJSON_Parse(line);
    free(line);

    return json;
}

//make an api GET call
int make_call(const char* url, char* hfilename, char* bfilename){
    CURL* curl;
    
    CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
    if(res)
        return (int)res;

    if(hfilename == NULL)
        hfilename = HEADERFILENAME;
    if(bfilename == NULL)
        bfilename = BODYFILENAME;

    //init curl session
    curl = curl_easy_init();
    FILE *headerfile = NULL;
    FILE *bodyfile = NULL;
    if(curl){
        //set the url
        curl_easy_setopt(curl, CURLOPT_URL, url);

        //follow rederection
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        //send data to write function
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);

        //open body file in binary mode
        bodyfile = fopen(bfilename, "wb");
        if(!bodyfile){
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return -1;
        }

        curl_easy_setopt(curl, CURLOPT_HEADERDATA, headerfile);
        //set file to write body to
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, bodyfile);

        //GET
        res = curl_easy_perform(curl);
        if(res != CURLE_OK){//error handle
            fprintf(stderr, "curl_easy_perform() failes: %s\n",
                    curl_easy_strerror(res));
        }
        fclose(bodyfile);
        curl_easy_cleanup(curl);//clenup curl
    }
    curl_global_cleanup();
    return (int)res;
}

//creates a url from a start interval in unix time 
char* make_url_from_interval(size_t start, char* symbol){

    //set the params
    char* param = malloc(sizeof(char) * 100);

    strcpy(param, "/klines?symbol=");
    strcat(param, symbol);
    strcat(param, "&interval=1d&startTime=");
    //

    //start time
    char* c_start = malloc(sizeof(char) * 14);

    sprintf(c_start, "%ld", start);
    //binance api takes unix time with miliseconds so we have to add the zeros
    strcat(c_start, "000");
    strcat(param, c_start);
    strcat(param, "&limit=1000");
    //

    //create full url
    char* url = malloc(strlen(BASE_API_URL) + strlen(param) + 1);
    strcpy(url, BASE_API_URL);
    strcat(url, param);
    //

    free(param);
    free(c_start);
    return url;
}

//append cJSON object to a file
void add_to_csv(cJSON* json, char* symbol, char* filename){
    if(filename == NULL){
        filename = CSV_FILENAME;
    }

    //if verbose is enabled
    if(VERBOSE == 0){
        printf("Adding data to csv for symbol %s\n", symbol);
    }

    //open to append
    FILE* csv = fopen(CSV_FILENAME, "a");
    if(csv == NULL){
        fprintf(stderr, "could not open file: %s\n", CSV_FILENAME);
        return;
    }

    cJSON* elem;
    char frm_date[100];

    cJSON_ArrayForEach(elem, json){
        int i = 0;
        cJSON* data;
        time_t date;
        float open_price, high_price, low_price,
              close_price, volume;

        //getting the data from the cJSON object
        cJSON_ArrayForEach(data, elem){
            switch(i){
                case 0:
                    date = data->valuedouble;
                break;
                case 1:
                    open_price = atof(data->valuestring);
                break;
                case 2:
                    high_price = atof(data->valuestring);                    
                case 3:
                    low_price = atof(data->valuestring);
                break;
                case 4:
                    close_price = atof(data->valuestring);
                break;
                case 5:
                    volume = atof(data->valuestring);
                break;
                default:
                break;
            }
            i++;
        }
        //divide by 1000 becouse binanace usese miliseconds in unix time
        date = date / 1000;
        
        //date formating
        struct tm *t = localtime(&date);
        strftime(frm_date, sizeof(frm_date), "%Y-%m-%d", t);

        //print to file
        fprintf(csv, "\n%s,%s,%s,%f,%f,%f,%f,%f,%f", symbol,
                symbol, frm_date, open_price,
                high_price, low_price, close_price, volume, close_price * volume);
    }
    fclose(csv);
}

//get historical data for symbol
void get_historical_data(char* symbol){
    time_t start = 1420070400; //jan 1 2015 in unix time
    time_t end = time(NULL); //current time

    size_t found_days = 0;
    //get all available historical data for symbol
    while(start <= end){
        char* url = make_url_from_interval(start, symbol);

        if(VERBOSE == 0){
            printf("getting historical data for %s\n", symbol);
            printf("from %s", asctime(gmtime(&start)));
        }

        make_call(url, NULL, NULL);
        free(url);

        //get the date of the last object
        cJSON* part = parse_to_json(NULL);
        cJSON* last_from_part = cJSON_GetArrayItem(part,
                cJSON_GetArraySize(part) - 1);

        time_t last_date = cJSON_GetArrayItem(last_from_part, 6)->valuedouble;
        //

        /*
         * make the start date the date of the last object from the previus call
         * this way we get all available data for symbol in the least amount of api calls
         */
        start = last_date / 1000;
        found_days += cJSON_GetArraySize(part);

        add_to_csv(part, symbol, NULL);
        cJSON_Delete(part);
    }

    if(VERBOSE == 0){
        printf("found %ld days for %s\n", found_days, symbol);
    }
}

int HAS_EXCHANGE_INFO = 1;
//returns a cJSON object with the exchange info
cJSON* get_exchange_info(){
    //create url
    const char* param = "/exchangeInfo";
    char* url = malloc(strlen(BASE_API_URL) + strlen(param) + 1);
    strcpy(url, BASE_API_URL);
    strcat(url, param);
    //

    if(VERBOSE == 0){
        printf("Getting exchangeInfo\n");
    }

    cJSON* json = NULL;
    //if arg --has-exchange-info is provided dont make an api call
    if(HAS_EXCHANGE_INFO == 1){
        make_call(url, "head.out", "body.out");
        json = parse_to_json("body.out");
    }else{
        json = parse_to_json("body.out");
    }

    free(url);
    return json;
}

int HAVE_HEAD = 0;
//creates the .csv file
void create_csv(){
    //open and truncate file
    FILE* csv = fopen(CSV_FILENAME, "w+");
    if(csv == NULL){
        fprintf(stderr, "could not open file: %s\n", CSV_FILENAME);
        return;
    }
    //if arg --no-csv-head is not provided write the head
    if(HAVE_HEAD == 0){
        fprintf(csv, "coin_id,symbol,date,open,high,low,close,volume,market_cap");
    }
    fclose(csv);
}

typedef struct allowed_assets_t{
    char** assets;//list of the names of allowed assets
    size_t size;//number of assets
} allowed_assets_t;

//frees memory allocated to allowed_assets_t*
void assets_delete(allowed_assets_t* aa){
    for(size_t i = 0;i < aa->size;i++){
        free(aa->assets[i]);
    }
    free(aa->assets);
    free(aa);
}

//creates the env file
void create_env(){
    FILE* f;
    f = fopen(ALLOWED_ASSET_ENV, "w");
    fprintf(f, "USDT");
    fclose(f);
}

//returs an object of allowed_assets_t with from the contets of the .allowed_assets env file
allowed_assets_t* assets_get(){
    allowed_assets_t* aa = malloc(sizeof *aa);
    //for storing asset names
    char* c_asset = malloc(sizeof(char) * 30);

    //create env file if it does not exist
    if(access(ALLOWED_ASSET_ENV, F_OK) != 0){
        create_env();
    }

    //count number of assets
    FILE* f;
    
    f = fopen(ALLOWED_ASSET_ENV, "r");
    size_t i = 0;
    while(fgets(c_asset, 30, f)){
        i++;
    }

    fclose(f);
    //

    aa->size = i;
    aa->assets = malloc(sizeof(char*) * aa->size);

    f = fopen(ALLOWED_ASSET_ENV, "r");

    for(i = 0;i < aa->size;i++){
        fgets(c_asset, 30, f);

        aa->assets[i] = malloc(sizeof(c_asset));
        strcpy(aa->assets[i], c_asset);

        //replace the new line charecter if there is one with null
        char* p = strchr(aa->assets[i], '\n');//finds first instance of '\n'
        if(p != NULL) *p = '\0';
    }

    fclose(f);
    free(c_asset);
    return aa;
}

//return 0 if char* c_asset is in the allowed_assets_t object
int asset_is_allowed(allowed_assets_t* aa, char* c_asset){
    if(aa == NULL || aa->assets == NULL){
        return 1;
    }

    for(size_t i = 0;i < aa->size;i++){
        if(aa->assets[i] == NULL){
            return 1;
        }
        if(strcmp(aa->assets[i], c_asset) == 0){
            return 0;
        }
    }
    return 1;
}

//prints contents of allowed_assets_t object
void assets_print(allowed_assets_t* aa){
    if(aa == NULL || aa->assets == NULL){
        return;
    }
    for(size_t i = 0;i < aa->size;i++){
        if(aa->assets[i] == NULL){
            printf("bad\n");
            return;
        }
        printf("%s\n", aa->assets[i]);
    }
}

//prints help message
void help(){
    printf("-h  --help\n");
    printf("\tshow this message\n\n");
    printf("--bfile\n");
    printf("\tset the name for the body file\n\n");
    printf("--hfile\n");
    printf("\tset the name for the header file (depreciated)\n\n");
    printf("--csvfile\n");
    printf("\tset the csv file\n\n");
    printf("--no-csv-head\n");
    printf("\twhen creating the csv file dont add the first line\n\n");
    printf("--has-exchange-info\n");
    printf("\tuse this tag if exchange info has already been collected\n\n");
    printf("--get-exchange-info\n");
    printf("\tjust get exchange info and do nothing else\n\n");
    printf("--get-number-of-coins\n");
    printf("\tget the number of valid coins from exchange info\n\n");
    printf("--from\n");
    printf("\tset which coin to start from (number)\n\n");
    printf("--to\n");
    printf("\tset which coin to end with (number)\n\n");
    printf("--verbose\n");
    printf("\texplain what is beeing done\n\n");
}

int main(int argc, char** argv){
    int from = 0;
    int to = 1000;

    int only_get_info = 1;
    int only_get_number_of_coins = 1;

    //arg handling
    for(int i = 0;i < argc;i++){
        if(strcmp(argv[i], "-h") == 0 || 
                strcmp(argv[i], "--help") == 0){
            help();
            return 0;
        }
        if(strcmp(argv[i], "--has-exchange-info") == 0){
            HAS_EXCHANGE_INFO = 0;
            continue;
        }
        if(strcmp(argv[i], "--get-exchange-info") == 0){
            only_get_info = 0;
            continue;
        }
        if(strcmp(argv[i], "--get-number-of-coins") == 0){
            only_get_number_of_coins = 0;
            continue;
        }
        if(strcmp(argv[i], "--no-csv-head") == 0){
            HAVE_HEAD = 1;
            continue;
        }
        if(strcmp(argv[i], "--verbose") == 0){
            VERBOSE = 0;
        }
        if(strcmp(argv[i], "--bfile") == 0){
            if(i + 1 >= argc){
                fprintf(stderr, "--bfile needs argument");
                return -1;
            }
            BODYFILENAME = argv[i + 1];
            continue;
        }
        if(strcmp(argv[i], "--hfile") == 0){
            if(i + 1 >= argc){
                fprintf(stderr, "--hfile needs argument");
                return -1;
            }
            HEADERFILENAME = argv[i + 1];
            continue;
        }
        if(strcmp(argv[i], "--csvfile") == 0){
            if(i + 1 >= argc){
                fprintf(stderr, "--csvfile needs argument");
                return -1;
            }
            CSV_FILENAME = argv[i + 1];
            continue;
        }
        if(strcmp(argv[i], "--from") == 0){
            if(i + 1 >= argc){
                fprintf(stderr, "--from needs argument");
                return -1;
            }
            from = atoi(argv[i + 1]);
        }
        if(strcmp(argv[i], "--to") == 0){
            if(i + 1 >= argc){
                fprintf(stderr, "--to needs argument");
                return -1;
            }
            to = atoi(argv[i + 1]);
        }
    }

    cJSON* json = get_exchange_info();

    if(only_get_info == 0){
        cJSON_Delete(json);
        return 0;
    }

    cJSON* symbols = cJSON_GetArrayItem(json,
            cJSON_GetArraySize(json) - 1);
    
    char c_symbols[cJSON_GetArraySize(symbols)][30];
    int i = 0;
    
    allowed_assets_t* aa = assets_get();

    cJSON* elem;
    //get only the symbols that trade with the allowed assets
    cJSON_ArrayForEach(elem, symbols){
        if(asset_is_allowed(aa, cJSON_GetObjectItem(elem, "quoteAsset")->valuestring) == 0 &&
                strcmp(cJSON_GetObjectItem(elem, "status")->valuestring, "TRADING") == 0){
            char* s = cJSON_GetObjectItem(elem, "symbol")->valuestring;
            strcpy(c_symbols[i], s);
            i++;
        }
    }

    assets_delete(aa);

    if(only_get_number_of_coins == 0){
        printf("%d\n", i);
        cJSON_Delete(json);
        return 0;
    }

    int size = i;
    i = from;
    create_csv();
    //get historical data for all the symbols in c_symbols
    while(i < to){
        if(i >= size){
            break;
        }
        if(VERBOSE == 0){
            printf("\n%d/%d\n", i, size);
        }
        get_historical_data(c_symbols[i]);
        i++;
    }

    cJSON_Delete(json);
}
