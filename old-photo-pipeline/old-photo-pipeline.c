/******************************************************************************
 * Programacao Concorrente
 * LEEC 2023/24
 *
 *
 * 	Grupo 46
 * 	João Simões 106070
 * 	Bernardo Pereira Lopes 106433
 * 	Projecto - Parte1
 * 	old-photo-pipeline.c
 *
 * Compilacao: gcc -pthread old-photo-pipeline.c image-lib.c image-lib.h -g -o old-photo-pipeline -lgd
 *
 *****************************************************************************/

#include <gd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <time.h>
#include "image-lib.h"
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>

#define maxLen 100
#define dMaxLen 2 * maxLen

int pipe_contrast[2];
int pipe_smoothed[2];
int pipe_textured[2];
int pipe_sepia[2];

typedef struct argFotos
{
    int indice;
    gdImagePtr produzida;
    struct timespec start_time_total;
} argFotos;

typedef struct arg_t
{
    char *OLD_IMAGE_DIR;
    char *textureImage;
    char **files;
    char *dataset;
    int notProcessed;
    FILE *timingFile;
} arg_t;

/******************************************************************************
 * Applytexture_stage1()
 *
 * Arguments: (void *arg)
 *
 * Description:loads a image from memory, applies a change to a image and saves it to a pipe
 *
 *
 *****************************************************************************/
void *ApplyContrast_stage1(void *arg)
{

    struct timespec start_time_image;
    gdImagePtr out_contrast_img;
    gdImagePtr in_img;
    arg_t *current = ((arg_t *)arg);
    char *openImage = (char *)malloc(maxLen * sizeof(char));
    char *nameFileAux = (char *)malloc(maxLen * sizeof(char));
    if (openImage == NULL || nameFileAux == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(-1);
    }
    char *dataset;
    char *OLD_IMAGE_DIR;
    char **files = current->files;
    char *inputName;
    int indice;

    argFotos *image;
    dataset = current->dataset;
    OLD_IMAGE_DIR = current->OLD_IMAGE_DIR;
    current->notProcessed = 0;

    while (read(pipe_contrast[0], &image, sizeof(image)) > 0)
    {
        // saves the start time of image
        clock_gettime(CLOCK_MONOTONIC, &start_time_image);
        struct timespec time_image = diff_timespec(&start_time_image, &image->start_time_total);

        inputName = files[image->indice];

        // load of the input file
        strcpy(openImage, dataset);
        strcat(openImage, inputName);

        in_img = read_jpeg_file(openImage);

        if (in_img == NULL)
        {
            fprintf(stderr, "Impossible to read %s image\n", openImage);

            continue;
        }
        // verifies if the image has already been processed
        sprintf(nameFileAux, "%s%s", OLD_IMAGE_DIR, inputName);
        if (access(nameFileAux, 0) == 0)
        {
            printf("File %s exists.\n", nameFileAux);
            current->notProcessed++;
            continue;
        }
        // writes the image start time to the file
        fprintf(current->timingFile, "%s start %10jd,%09ld\n", files[image->indice], time_image.tv_sec, time_image.tv_nsec);

        // makes the changes to the images
        out_contrast_img = contrast_image(in_img);
        gdImageDestroy(in_img);
        // writes the image information in to the pipe
        image->produzida = out_contrast_img;
        if (write(pipe_smoothed[1], &image, sizeof(image)) == -1)
        {
            fprintf(stderr, " Impossible to write on pipe");
            exit(-1);
        }
    }
    close(pipe_smoothed[1]);
    free(openImage);
    free(nameFileAux);

    return NULL;
}
/******************************************************************************
 * Applytexture_stage2()
 *
 * Arguments: (void *arg)
 *
 * Description:reads a image from a pipe, applies a change to a image and saves it to a pipe
 *
 *
 *****************************************************************************/

void *Applysmoothed_stage2(void *arg)
{

    gdImagePtr out_smoothed_img;
    gdImagePtr out_contrast_img;

    arg_t *current = ((arg_t *)arg);

    char *tempo = (char *)malloc(dMaxLen * sizeof(char));
    if (tempo == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(-1);
    }
    char *dataset;
    char *OLD_IMAGE_DIR;
    char **files = current->files;
    char *aux = (char *)malloc(256 * sizeof(char));
    int indice;
    argFotos *image;

    dataset = current->dataset;
    OLD_IMAGE_DIR = current->OLD_IMAGE_DIR;

    while (read(pipe_smoothed[0], &image, sizeof(image)) > 0)
    {
        // makes the changes to the images
        out_contrast_img = image->produzida;
        out_smoothed_img = smooth_image(out_contrast_img);
        gdImageDestroy(out_contrast_img);
        image->produzida = out_smoothed_img;
        // write the image information on the  pipe
        if (write(pipe_textured[1], &image, sizeof(image)) == -1)
        {
            fprintf(stderr, " Impossible to write on pipe");
            exit(-1);
        }
    }
    close(pipe_textured[1]);

    return NULL;
}
/******************************************************************************
 * Applytexture_stage3()
 *
 * Arguments: (void *arg)
 *
 * Description:reads a image from a pipe, applies a change to a image and saves it to a pipe
 *
 *
 *****************************************************************************/
void *Applytexture_stage3(void *arg)
{

    gdImagePtr out_smoothed_img;
    gdImagePtr out_textured_img;
    gdImagePtr in_texture_img;
    arg_t *current = ((arg_t *)arg);
    char *dataset;
    char *OLD_IMAGE_DIR;
    char **files = current->files;
    argFotos *image;

    dataset = current->dataset;
    OLD_IMAGE_DIR = current->OLD_IMAGE_DIR;

    in_texture_img = read_png_file(current->textureImage);

    if (in_texture_img == NULL)
    {
        fprintf(stderr, "Impossible to read %s image\n", current->textureImage);
        exit(-1);
    }

    while (read(pipe_textured[0], &image, sizeof(image)) > 0)
    {
        // makes the changes to the images
        out_smoothed_img = image->produzida;
        out_textured_img = texture_image(out_smoothed_img, in_texture_img);
        gdImageDestroy(out_smoothed_img);
        // writes the image information on the pipe
        image->produzida = out_textured_img;
        if (write(pipe_sepia[1], &image, sizeof(image)) == -1)
        {
            fprintf(stderr, " Impossible to write on pipe");
            exit(-1);
        }
    }
    close(pipe_sepia[1]);
    gdImageDestroy(in_texture_img);

    return NULL;
}
/******************************************************************************
 * ApplySepia_stage4()
 *
 * Arguments: (void *arg)
 *
 * Description:reads a image from a pipe, applies a change to a image and saves it to memmory
 *
 *
 *****************************************************************************/

void *ApplySepia_stage4(void *arg)
{

    struct timespec end_time_image;
    gdImagePtr out_textured_img;
    gdImagePtr out_sepia_img;
    arg_t *current = ((arg_t *)arg);
    argFotos *image;
    char *out_file_name = (char *)malloc(maxLen * sizeof(char));
    char *dataset;
    char *OLD_IMAGE_DIR;
    char **files = current->files;

    dataset = current->dataset;
    OLD_IMAGE_DIR = current->OLD_IMAGE_DIR;

    if (out_file_name == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(-1);
    }

    while (read(pipe_sepia[0], &image, sizeof(image)) > 0)
    {

        // makes the changes to the images
        out_textured_img = image->produzida;
        out_sepia_img = sepia_image(out_textured_img);
        gdImageDestroy(out_textured_img);

        /* save resized */
        sprintf(out_file_name, "%s%s", OLD_IMAGE_DIR, files[image->indice]);
        if (write_jpeg_file(out_sepia_img, out_file_name) == 0)
        {
            fprintf(stderr, "Impossible to write %s image\n", out_file_name);
        }
        gdImageDestroy(out_sepia_img);
        // writes end time of image to the file
        clock_gettime(CLOCK_MONOTONIC, &end_time_image);
        struct timespec time_image = diff_timespec(&end_time_image, &image->start_time_total);
        fprintf(current->timingFile, "%s end %10jd,%09ld\n", files[image->indice], time_image.tv_sec, time_image.tv_nsec);
    }

    free(out_file_name);
    return NULL;
}
/******************************************************************************
 * main()
 *
 * Arguments: (int argc, char *argv[])
 * Returns: 0 in case of sucess, positive number in case of failure
 *
 *
 * Description: Distributes the workload of processing files among the threads.
 *
 *

 *****************************************************************************/

int main(int argc, char *argv[])

{

    struct timespec start_time_total, end_time_total;
    struct timespec start_time_seq, end_time_seq;
    struct timespec start_time_par, end_time_par;

    clock_gettime(CLOCK_MONOTONIC, &start_time_total);
    clock_gettime(CLOCK_MONOTONIC, &start_time_seq);

    if (argc != 2)
    {
        fprintf(stderr, "Wrong invocation of program\n Usage {program} {directory}");
        return 0;
    }

    char *out_file_name = (char *)malloc(maxLen * sizeof(char));
    char *imageNameFile = (char *)malloc(maxLen * sizeof(char));
    char *buffer = (char *)malloc(maxLen * sizeof(char));
    char *OLD_IMAGE_DIR = (char *)malloc(maxLen * sizeof(char));
    char **files;
    char *dataset = (char *)malloc(maxLen * sizeof(char));
    char *textura = (char *)malloc(dMaxLen * sizeof(char));
    char **temposPerThread;
    char *nameTimingFile = (char *)malloc(dMaxLen * sizeof(char));
    if (out_file_name == NULL || imageNameFile == NULL || buffer == NULL ||
        OLD_IMAGE_DIR == NULL || dataset == NULL || textura == NULL || nameTimingFile == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(-1);
    }

    FILE *imageTxtFile;
    FILE *timingFile;

    int nFiles = 0;
    int allNotProcessed = 0;

    void *thread_ret;

    arg_t *threadsArg;

    argFotos **images;
    pthread_t *threadsIdContrast;
    pthread_t *threadsIdSmothed;
    pthread_t *threadsIdTexture;
    pthread_t *threadsIdSepia;

    if (pipe(pipe_contrast) != 0 || pipe(pipe_smoothed) != 0 || pipe(pipe_textured) != 0 || pipe(pipe_sepia) != 0)
    {
        printf("error creating the pipes");
        exit(-1);
    }

    // directorie where the images are
    strcpy(dataset, argv[1]);
    strcat(dataset, "/");

    // name of the output directory
    strcpy(OLD_IMAGE_DIR, dataset);
    strcat(OLD_IMAGE_DIR, "old_photo_PIPELINE/");

    // name of the input list directory
    strcpy(imageNameFile, dataset);
    strcat(imageNameFile, "image-list.txt");

    // write of out timing file
    sprintf(nameTimingFile, "%stiming_pipeline.txt", dataset);
    timingFile = fopen(nameTimingFile, "w");

    // read of image names file and memory allocation for the names of the images
    imageTxtFile = fopen(imageNameFile, "r");
    if (imageTxtFile == NULL)
    {
        fprintf(stderr, "Error opening file: %s\n", imageNameFile);
        exit(-1);
    }
    while (fscanf(imageTxtFile, "%s", buffer) == 1)
    {
        nFiles++;
    }
    rewind(imageTxtFile);
    images = (argFotos **)malloc(nFiles * sizeof(argFotos *));
    files = (char **)malloc(nFiles * sizeof(char *));
    if (files == NULL)
    {
        fprintf(stderr, "Memory allocation failed for 'files'\n");
        exit(-1);
    }
    int i = 0;
    while (fscanf(imageTxtFile, "%s", buffer) == 1)
    {
        files[i] = (char *)malloc(256 * sizeof(char));
        images[i] = (argFotos *)malloc(sizeof(argFotos));
        images[i]->indice = i;
        images[i]->start_time_total = start_time_total;
        if (files[i] == NULL)
        {
            fprintf(stderr, "Memory allocation failed for 'files[%d]'\n", i);
            exit(-1);
        }
        strcpy(files[i], buffer);
        i++;
    }
    fclose(imageTxtFile);

    /* creation of output directory*/
    if (create_directory(OLD_IMAGE_DIR) == 0)
    {
        fprintf(stderr, "Impossible to create %s directory\n", OLD_IMAGE_DIR);
        exit(-1);
    }

    // verification of textured image
    strcpy(textura, "./paper-texture.png");
    if (access(textura, 0) != 0)
    {
        sprintf(textura, "%s%s", dataset, "paper-texture.png");
    }

    // allocate memory for threads arguments and threads id
    threadsArg = (arg_t *)malloc(sizeof(arg_t));
    threadsIdContrast = (pthread_t *)malloc(sizeof(pthread_t));
    threadsIdSmothed = (pthread_t *)malloc(sizeof(pthread_t));
    threadsIdTexture = (pthread_t *)malloc(sizeof(pthread_t));
    threadsIdSepia = (pthread_t *)malloc(sizeof(pthread_t));
    if (threadsArg == NULL)
    {
        fprintf(stderr, "Memory allocation failed for 'threadsArg' or 'threadsId'\n");
        exit(EXIT_FAILURE);
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time_seq);
    clock_gettime(CLOCK_MONOTONIC, &start_time_par);

    // call of threads
    threadsArg->OLD_IMAGE_DIR = OLD_IMAGE_DIR;
    threadsArg->textureImage = textura;
    threadsArg->dataset = dataset;
    threadsArg->files = files;
    threadsArg->timingFile = timingFile;
    pthread_create(threadsIdContrast, NULL, ApplyContrast_stage1, (void *)threadsArg); // primeiro argumento referencia para o id da thread
    pthread_create(threadsIdSmothed, NULL, Applysmoothed_stage2, (void *)threadsArg);
    pthread_create(threadsIdTexture, NULL, Applytexture_stage3, (void *)threadsArg);
    pthread_create(threadsIdSepia, NULL, ApplySepia_stage4, (void *)threadsArg);

    // write to the threads
    for (int i = 0; i < nFiles; i++)
    {
        if (write(pipe_contrast[1], &images[i], sizeof(images[i])) == -1)
        {
            fprintf(stderr, " Impossible to write on pipe");
            exit(-1);
        }
    }
    close(pipe_contrast[1]);

    // join of threads

    pthread_join(*threadsIdContrast, &thread_ret); // primeiro argumento recebe o valor do id da thread
    pthread_join(*threadsIdSmothed, &thread_ret);
    pthread_join(*threadsIdTexture, &thread_ret);
    pthread_join(*threadsIdSepia, &thread_ret);
    allNotProcessed = allNotProcessed + threadsArg->notProcessed;

    free(threadsIdContrast);
    free(threadsIdSmothed);
    free(threadsIdTexture);
    free(threadsIdSepia);
    free(out_file_name);
    free(imageNameFile);
    free(buffer);
    free(OLD_IMAGE_DIR);

    clock_gettime(CLOCK_MONOTONIC, &end_time_par);
    clock_gettime(CLOCK_MONOTONIC, &end_time_total);
    struct timespec par_time = diff_timespec(&end_time_par, &start_time_par);
    struct timespec seq_time = diff_timespec(&end_time_seq, &start_time_seq);
    struct timespec total_time = diff_timespec(&end_time_total, &start_time_total);
    printf("\tseq \t %10jd.%09ld\n", seq_time.tv_sec, seq_time.tv_nsec);
    printf("\tpar \t %10jd.%09ld\n", par_time.tv_sec, par_time.tv_nsec);
    printf("total \t  %10jd.%09ld\n", total_time.tv_sec, total_time.tv_nsec);
    // writing total time of program on timing file
    if (timingFile == NULL)
    {
        fprintf(stderr, "Error opening file: %s\n", nameTimingFile);
        exit(-1);
    }
    fprintf(timingFile, "total \t  %10jd,%09ld\n", total_time.tv_sec, total_time.tv_nsec);
    // free all memory
    for (int i = 0; i < nFiles; i++)
    {
        free(files[i]);
        free(images[i]);
    }
    free(images);
    free(files);
    fclose(timingFile);
    free(nameTimingFile);
    free(threadsArg);
    exit(0);
}