#include <stdio.h>
#include <stdlib.h> 
#include <errno.h>  
#include <string.h>
#include <dirent.h>

#include "png_util/crc.c" 
#include "png_util/lab_png.h" 

#define BUFFER_LENGTH 255

int listFilesRecursively(char *path);


int main(int argc, char *argv[]) {
    // Directory path to list files
	if (argc == 1) {
        printf("Usage: %s png_file\n", argv[0]);
        return -1;
    }

    U8 path[255];

    strcpy(path, argv[1]);
    int numFiles = listFilesRecursively(path);
    if (numFiles == 0){
    	printf("findpng: No PNG file found\n");
    }
    else if (numFiles == -1){
	printf("Not a directory\n");
    }
	

    return 0;
}


/**
 * Lists all files and sub-directories recursively 
 * considering path as base path.
 */
 
int listFilesRecursively(char *basePath){
    int filesFound = 0;
    char path[1000];
    struct dirent *dp;
    DIR *dir = opendir(basePath);

    // Unable to open directory stream
    if (!dir){
        return -1;
	}
	
	U8 signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
	
	char *pngext = ".png";
	//while there dir is not NULL, meaning there is still more to the directory
    while ((dp = readdir(dir)) != NULL){
		//if its not . or .. 
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0){

			if(strstr(dp->d_name, pngext)){
				//Check if actually png using header, then print
				/* Open png */
				char pngfile[255];
				strcpy(pngfile, basePath);
				strcat(pngfile, "/");
				strcat(pngfile, dp->d_name); 
				FILE *png = fopen(pngfile, "rt");
				int isPNG; 
				U8 buffer[BUFFER_LENGTH];
				U8 header[8];
				memset(buffer, 0, BUFFER_LENGTH);
				fread(buffer, sizeof(buffer), 1, png);
				for(int i = 0; i < 8; i++){
					header[i] = buffer[i];
				}
				if (isPNG = !memcmp(signature, header, sizeof(signature))){
					printf("%s\n", dp->d_name);
					filesFound++;
				}		
			}
          
            // Construct new path from our base path
            strcpy(path, basePath);
            strcat(path, "/");
            strcat(path, dp->d_name);

            listFilesRecursively(path);
        }
    }

    closedir(dir);
    return filesFound;
}



