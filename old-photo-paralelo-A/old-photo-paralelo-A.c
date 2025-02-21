/******************************************************************************
 * Programacao Concorrente
 * LEEC 2023/24
 *
 *
 * 	Grupo 46
 * 	João Simões 106070
 * 	Bernardo Pereira Lopes 106433
 * 	Projecto - Parte1
 * 	old-photo-paralelo-A.c
 *
 * Compilacao: gcc -pthread old-photo-paralelo-A.c image-lib.c image-lib.h -g -o old-photo-paralelo-A -lgd
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

typedef struct arg_t
{
	int start;
	int end;
	char *OLD_IMAGE_DIR;
	char *textureImage;
	char **files;
	char *dataset;
	int notProcessed;
} arg_t;

/******************************************************************************
 * functionImages()
 *
 * Arguments: (void *arg)
 * Returns: the time the thread takes to produce the images
 * Description: creates thumbnail, resized copy and watermarked copies
 *               of images
 *
 *****************************************************************************/

void *functionImages(void *arg)
{

	struct timespec start_time_th, end_time_th;
	clock_gettime(CLOCK_MONOTONIC, &start_time_th);
	gdImagePtr out_smoothed_img;
	gdImagePtr out_contrast_img;
	gdImagePtr out_textured_img;
	gdImagePtr out_sepia_img;
	gdImagePtr in_texture_img;
	gdImagePtr in_img;

	arg_t *current = ((arg_t *)arg);

	char *openImage = (char *)malloc(maxLen * sizeof(char));
	char *out_file_name = (char *)malloc(maxLen * sizeof(char));
	char *tempo = (char *)malloc(dMaxLen * sizeof(char));
	char *nameFileAux = (char *)malloc(maxLen * sizeof(char));
	if (openImage == NULL || out_file_name == NULL || tempo == NULL || nameFileAux == NULL)
	{
		fprintf(stderr, "Memory allocation failed\n");
		exit(-1);
	}
	char *dataset;
	char *OLD_IMAGE_DIR;
	char **files;

	int position;

	dataset = current->dataset;
	OLD_IMAGE_DIR = current->OLD_IMAGE_DIR;
	files = current->files;
	current->notProcessed = 0;
	position = current->start;

	in_texture_img = read_png_file(current->textureImage);

	if (in_texture_img == NULL)
	{
		fprintf(stderr, "Impossible to read %s image\n", current->textureImage);
		exit(-1);
	}

	// verifies if there are images to process
	if (position == -1)
	{
		free(openImage);
		free(out_file_name);
		free(nameFileAux);
		clock_gettime(CLOCK_MONOTONIC, &end_time_th);
		struct timespec total_time_th = diff_timespec(&end_time_th, &start_time_th);
		sprintf(tempo, "%10jd.%09ld\n", total_time_th.tv_sec, total_time_th.tv_nsec);
		return (void *)tempo;
	}

	while (position != current->end + 1)
	{
		// load of the input file
		strcpy(openImage, dataset);
		strcat(openImage, files[position]);

		in_img = read_jpeg_file(openImage);

		if (in_img == NULL)
		{
			fprintf(stderr, "Impossible to read %s image\n", openImage);
			position++;
			continue;
		}
		// verifies if the image has already been processed
		sprintf(nameFileAux, "%s%s", OLD_IMAGE_DIR, files[position]);
		if (access(nameFileAux, 0) == 0)
		{
			printf("File %s exists.\n", nameFileAux);
			position++;
			current->notProcessed++;
			continue;
		}
		// makes the changes to the images
		out_contrast_img = contrast_image(in_img);
		gdImageDestroy(in_img);
		out_smoothed_img = smooth_image(out_contrast_img);
		gdImageDestroy(out_contrast_img);
		out_textured_img = texture_image(out_smoothed_img, in_texture_img);
		gdImageDestroy(out_smoothed_img);
		out_sepia_img = sepia_image(out_textured_img);
		gdImageDestroy(out_textured_img);

		/* save resized */
		sprintf(out_file_name, "%s%s", OLD_IMAGE_DIR, files[position]);
		if (write_jpeg_file(out_sepia_img, out_file_name) == 0)
		{
			fprintf(stderr, "Impossible to write %s image\n", out_file_name);
		}
		gdImageDestroy(out_sepia_img);
		position++;
	}
	gdImageDestroy(in_texture_img);
	free(openImage);
	free(out_file_name);
	free(nameFileAux);
	clock_gettime(CLOCK_MONOTONIC, &end_time_th);
	struct timespec total_time_th = diff_timespec(&end_time_th, &start_time_th);
	sprintf(tempo, "%10jd,%09ld\n", total_time_th.tv_sec, total_time_th.tv_nsec);
	return (void *)tempo;
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

	if (argc != 3)
	{
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

	int n_thread;
	int pivot = 0;
	int filePerThread;
	int resto;
	int nFotos;
	int nFiles = 0;
	int allNotProcessed = 0;

	void *thread_ret;

	arg_t *threadsArg;
	pthread_t *threadsId;

	n_thread = atoi(argv[2]);

	// directorie where the images are
	strcpy(dataset, argv[1]);
	strcat(dataset, "/");

	// name of the output directory
	strcpy(OLD_IMAGE_DIR, dataset);
	strcat(OLD_IMAGE_DIR, "old_photo_PAR_A/");

	// name of the input list directory
	strcpy(imageNameFile, dataset);
	strcat(imageNameFile, "image-list.txt");

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
	threadsArg = (arg_t *)malloc(n_thread * sizeof(arg_t));
	threadsId = (pthread_t *)malloc(n_thread * sizeof(pthread_t));
	if (threadsArg == NULL || threadsId == NULL)
	{
		fprintf(stderr, "Memory allocation failed for 'threadsArg' or 'threadsId'\n");
		exit(EXIT_FAILURE);
	}

	// Division of files among the threads.
	filePerThread = nFiles / n_thread;
	resto = nFiles % n_thread;
	for (int i = 0; i < n_thread; i++)
	{
		if (pivot > nFiles - 1)
		{
			threadsArg[i].start = -1;
			threadsArg[i].end = -1;
			continue;
		}
		threadsArg[i].start = pivot;
		threadsArg[i].end = pivot + filePerThread - 1;
		if (resto > 0)
		{
			threadsArg[i].end = pivot + filePerThread;
			resto--;
			pivot = pivot + filePerThread + 1;
			continue;
		}
		pivot += filePerThread;
	}

	clock_gettime(CLOCK_MONOTONIC, &end_time_seq);
	clock_gettime(CLOCK_MONOTONIC, &start_time_par);

	// call of threads
	for (int i = 0; i < n_thread; i++)
	{
		threadsArg[i].OLD_IMAGE_DIR = OLD_IMAGE_DIR;
		threadsArg[i].files = files;
		threadsArg[i].textureImage = textura;
		threadsArg[i].dataset = dataset;
		pthread_create(&threadsId[i], NULL, functionImages, (void *)&threadsArg[i]);
	}

	// join of threads
	temposPerThread = (char **)malloc(n_thread * sizeof(char *));
	if (temposPerThread == NULL)
	{
		fprintf(stderr, "Memory allocation failed for 'temposPerThread'\n");
		exit(-1);
	}
	for (int i = 0; i < n_thread; i++)
	{
		temposPerThread[i] = (char *)malloc(n_thread * sizeof(char));
		if (temposPerThread[i] == NULL)
		{
			fprintf(stderr, "Memory allocation failed for 'temposPerThread[%d]'\n", i);
			exit(-1);
		}
		pthread_join(threadsId[i], &thread_ret);
		strcpy(temposPerThread[i], (char *)thread_ret);
		allNotProcessed = allNotProcessed + threadsArg[i].notProcessed;
		free((char *)thread_ret);
	}
	// freee
	for (int i = 0; i < nFiles; i++)
		free(files[i]);
	free(files);
	free(threadsId);
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

	// write of out timing file
	sprintf(nameTimingFile, "%stiming_%d.txt", dataset, n_thread);
	timingFile = fopen(nameTimingFile, "w");
	if (timingFile == NULL)
	{
		fprintf(stderr, "Error opening file: %s\n", nameTimingFile);
		exit(-1);
	}
	fprintf(timingFile, "total \t %d %10jd,%09ld\n", nFiles - allNotProcessed, total_time.tv_sec, total_time.tv_nsec);
	for (int i = 0; i < n_thread; i++)
	{
		nFotos = 0;
		if (threadsArg[i].end != -1)
			nFotos = (threadsArg[i].end - threadsArg[i].start) + 1 - threadsArg[i].notProcessed;
		fprintf(timingFile, "Thread_%d %d %s", i, nFotos, temposPerThread[i]);
		free(temposPerThread[i]);
	}

	fclose(timingFile);
	free(nameTimingFile);
	free(threadsArg);
	free(temposPerThread);
	exit(0);
}
