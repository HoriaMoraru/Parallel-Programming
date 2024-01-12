// Author: APD team, except where source was noted

#include "helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define CONTOUR_CONFIG_COUNT 16
#define FILENAME_MAX_SIZE 50
#define STEP 8
#define SIGMA 200
#define RESCALE_X 2048
#define RESCALE_Y 2048

#define CLAMP(v, min, max) \
  if (v < min)             \
  {                        \
    v = min;               \
  }                        \
  else if (v > max)        \
  {                        \
    v = max;               \
  }

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

typedef struct Thread
{
  int id;
  int start;
  int end;
} Thread;

typedef struct Info
{
  int step_x;
  int step_y;
  unsigned char **grid;
  ppm_image *image;
  ppm_image *rescaled_image;
  ppm_image **contour_map;
  pthread_barrier_t *barrier;
  int should_rescale;
  int num_threads;
} Info;

typedef struct Thread_Args
{
  Thread curr_thread;
  Info *threads_info;
} Thread_Args;

void *thread_function(void *args)
{
  Thread_Args *data = (Thread_Args *)args;
  Info *g_info = data->threads_info;
  int s = data->curr_thread.start;
  int e = data->curr_thread.end;
  int t_id = data->curr_thread.id;

  // Start of rescale phase
  uint8_t sample[3]; // temporary storage for sampled pixels
  if (g_info->should_rescale)
  {
    for (int i = s; i < e; ++i)
    {
      pthread_barrier_wait(g_info->barrier);
      for (int j = 0; j < g_info->rescaled_image->y; ++j)
      {
        float u = (float)i / (float)(g_info->rescaled_image->x - 1);
        float v = (float)j / (float)(g_info->rescaled_image->y - 1);
        sample_bicubic(g_info->image, u, v, sample);

        g_info->rescaled_image->data[i * g_info->rescaled_image->y + j].red = sample[0];
        g_info->rescaled_image->data[i * g_info->rescaled_image->y + j].green = sample[1];
        g_info->rescaled_image->data[i * g_info->rescaled_image->y + j].blue = sample[2];
      }
    }
  }
  //  End of rescale phase

  // Start of sample grid phase
  int p = g_info->rescaled_image->x / g_info->step_x;
  int q = g_info->rescaled_image->y / g_info->step_y;

  int s_p = t_id * (p / g_info->num_threads);
  int e_p = MIN((t_id + 1) * (p / g_info->num_threads), p);

  int s_q = t_id * (q / g_info->num_threads);
  int e_q = MIN((t_id + 1) * (q / g_info->num_threads), q);

  for (int i = s_p; i < e_p; ++i)
  {
    for (int j = 0; j < q; ++j)
    {
      ppm_pixel curr_pixel = g_info->rescaled_image->data[i * g_info->step_x * g_info->rescaled_image->y + j * g_info->step_y];

      unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

      if (curr_color > SIGMA)
      {
        g_info->grid[i][j] = 0;
      }
      else
      {
        g_info->grid[i][j] = 1;
      }
    }
  }
  g_info->grid[p][q] = 0;

  // last sample points have no neighbors below / to the right, so we use pixels on the
  // last row / column of the input image for them
  for (int i = s_p; i < e_p; ++i)
  {

    ppm_pixel curr_pixel = g_info->rescaled_image->data[i * g_info->step_x * g_info->rescaled_image->y + g_info->rescaled_image->x - 1];

    unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

    if (curr_color > SIGMA)
    {
      g_info->grid[i][q] = 0;
    }
    else
    {
      g_info->grid[i][q] = 1;
    }
  }

  for (int j = s_q; j < e_q; ++j)
  {
    ppm_pixel curr_pixel = g_info->rescaled_image->data[(g_info->rescaled_image->x - 1) * g_info->rescaled_image->y + j * g_info->step_y];

    unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

    if (curr_color > SIGMA)
    {
      g_info->grid[p][j] = 0;
    }
    else
    {
      g_info->grid[p][j] = 1;
    }
  }
  // End of sample grid phase

  // Start of march phase

  for (int i = s_p; i < e_p; ++i)
  {
    for (int j = 0; j < q; ++j)
    {
      unsigned char k = 8 * g_info->grid[i][j] + 4 * g_info->grid[i][j + 1] + 2 * g_info->grid[i + 1][j + 1] + 1 * g_info->grid[i + 1][j];

      int update_x = i * g_info->step_x;
      int update_y = j * g_info->step_y;
      int update_width = g_info->step_x;
      int update_height = g_info->step_y;

      update_image_parallel(g_info->rescaled_image, g_info->contour_map[k], update_x, update_y, update_width, update_height);
    }
  }

  // End of thread
  pthread_exit(NULL);
}

// Updates a particular section of an image with the corresponding contour pixels.
// Used to create the complete contour image.
void update_image_parallel(ppm_image *image, ppm_image *contour, int x, int y, int width, int height)
{
  for (int i = 0; i < width; i++)
  {
    for (int j = 0; j < height; j++)
    {
      int contour_pixel_index = contour->x * i + j;
      int image_pixel_index = (x + i) * image->y + y + j;

      image->data[image_pixel_index].red = contour->data[contour_pixel_index].red;
      image->data[image_pixel_index].green = contour->data[contour_pixel_index].green;
      image->data[image_pixel_index].blue = contour->data[contour_pixel_index].blue;
    }
  }
}

// Creates a map between the binary configuration (e.g. 0110_2) and the corresponding pixels
// that need to be set on the output image. An array is used for this map since the keys are
// binary numbers in 0-15. Contour images are located in the './contours' directory.
ppm_image **init_contour_map()
{
  ppm_image **map = (ppm_image **)malloc(CONTOUR_CONFIG_COUNT * sizeof(ppm_image *));
  if (!map)
  {
    fprintf(stderr, "Unable to allocate memory\n");
    exit(1);
  }

  for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++)
  {
    char filename[FILENAME_MAX_SIZE];
    sprintf(filename, "./contours/%d.ppm", i);
    map[i] = read_ppm(filename);
  }

  return map;
}

// Allocates memory for the grid of points which will be sampled from the original image.
unsigned char **alloc_grid(ppm_image *image, int step_x, int step_y)
{
  int p = image->x / step_x;
  int q = image->y / step_y;

  unsigned char **grid = (unsigned char **)malloc((p + 1) * sizeof(unsigned char *));
  if (!grid)
  {
    fprintf(stderr, "Unable to allocate memory\n");
    exit(1);
  }

  for (int i = 0; i <= p; i++)
  {
    grid[i] = (unsigned char *)malloc((q + 1) * sizeof(unsigned char));
    if (!grid[i])
    {
      fprintf(stderr, "Unable to allocate memory\n");
      exit(1);
    }
  }

  return grid;
}

// Allocates memory for the rescaled image, if needed.
ppm_image *alloc_image(ppm_image *image, int *rescale)
{
  // we only rescale downwards
  if (image->x <= RESCALE_X && image->y <= RESCALE_Y)
  {
    return image;
  }

  // alloc memory for image
  ppm_image *rescaled_image = (ppm_image *)malloc(sizeof(ppm_image));
  if (!rescaled_image)
  {
    fprintf(stderr, "Unable to allocate memory\n");
    exit(1);
  }
  rescaled_image->x = RESCALE_X;
  rescaled_image->y = RESCALE_Y;

  rescaled_image->data = (ppm_pixel *)malloc(rescaled_image->x * rescaled_image->y * sizeof(ppm_pixel));
  if (!rescaled_image)
  {
    fprintf(stderr, "Unable to allocate memory\n");
    exit(1);
  }

  *rescale = 1;

  return rescaled_image;
}

void create_info(Info *info, int step_x, int step_y, unsigned char **grid, ppm_image *image, ppm_image *rescaled_image, ppm_image **contour_map, pthread_barrier_t *barrier, int should_rescale, int num_threads)
{
  info->step_x = step_x;
  info->step_y = step_y;
  info->grid = grid;
  info->image = image;
  info->rescaled_image = rescaled_image;
  info->contour_map = contour_map;
  info->barrier = barrier;
  info->should_rescale = should_rescale;
  info->num_threads = num_threads;
}

// Calls `free` method on the utilized resources.
void free_resources(Info *info)
{
  for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++)
  {
    free(info->contour_map[i]->data);
    free(info->contour_map[i]);
  }
  free(info->contour_map);

  for (int i = 0; i <= info->rescaled_image->x / info->step_x; i++)
  {
    free(info->grid[i]);
  }
  free(info->grid);

  free(info->rescaled_image->data);
  free(info->rescaled_image);

  if (info->should_rescale)
  {
    free(info->image->data);
    free(info->image);
  }

  pthread_barrier_destroy(info->barrier);

  free(info);
}

int main(int argc, char *argv[])
{
  if (argc < 4)
  {
    fprintf(stderr, "Usage: ./tema1 <in_file> <out_file> <P>\n");
    return 1;
  }

  int r, t_id, rescale = 0, step_x = STEP, step_y = STEP;
  pthread_barrier_t barrier;
  const int NUM_THREADS = atoi(argv[3]);
  pthread_t threads[NUM_THREADS];
  Thread_Args arguments[NUM_THREADS];
  Info *info = (Info *)malloc(sizeof(Info));

  ppm_image *image = read_ppm(argv[1]);
  ppm_image *rescaled_image = alloc_image(image, &rescale);
  unsigned char **grid = alloc_grid(rescaled_image, step_x, step_y);

  // Initialize contour map
  ppm_image **contour_map = init_contour_map();

  create_info(info, step_x, step_y, grid, image, rescaled_image, contour_map, &barrier, rescale, NUM_THREADS);

  r = pthread_barrier_init(&barrier, NULL, NUM_THREADS);

  if (r)
  {
    fprintf(stderr, "Eroare la initializarea barierei\n");
    exit(-1);
  }

  for (t_id = 0; t_id < NUM_THREADS; ++t_id)
  {
    arguments[t_id].threads_info = info;
    arguments[t_id].curr_thread.id = t_id;
    arguments[t_id].curr_thread.start = t_id * (rescaled_image->x / NUM_THREADS);
    arguments[t_id].curr_thread.end = MIN((t_id + 1) * (rescaled_image->x / NUM_THREADS), rescaled_image->x);
    r = pthread_create(&threads[t_id], NULL, thread_function, (void *)&arguments[t_id]);

    if (r)
    {
      exit(-1);
    }
  }

  for (t_id = 0; t_id < NUM_THREADS; ++t_id)
  {
    r = pthread_join(threads[t_id], NULL);

    if (r)
    {
      exit(-1);
    }
  }

  // Write output
  write_ppm(rescaled_image, argv[2]);

  // Free resources
  free_resources(info);

  return 0;
}
