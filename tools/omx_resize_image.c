#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <OMX_Core.h>

#include <bcm_host.h>

#include "component.h"

// 5MB output buffer.
#define OUTPUT_BUFFER_SIZE (1024*1024*5)


void error_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
  fprintf(stderr, "OMX error 0x%X\n", (unsigned int)data);
}

void eos_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
  fprintf(stderr, "Got EOS event 0x%X\n", (unsigned int)data);
}

OMX_ERRORTYPE read_into_buffer_and_empty(FILE *f, COMPONENT_T *component,
    OMX_BUFFERHEADERTYPE *buff_header, int *bytes_to_read) {

  int buff_size = buff_header->nAllocLen;
  int nread = fread(buff_header->pBuffer, 1, buff_size, f);
  *bytes_to_read -= nread;

  fprintf(stderr, "Read %d bytes\n", nread);

  buff_header->nFilledLen = nread;
  if (*bytes_to_read <= 0) {
    fprintf(stderr, "Setting EOS on input\n");
    buff_header->nFlags |= OMX_BUFFERFLAG_EOS;
  }

  OMX_ERRORTYPE err = OMX_EmptyThisBuffer(ilclient_get_handle(component), buff_header);
  if (err != OMX_ErrorNone) {
    fprintf(stderr, "Empty buffer error 0x%X\n", (unsigned int)err);
  }

  return err;
}

bool process_output_chunk(COMPONENT_T *component,
    OMX_BUFFERHEADERTYPE *buff_header, unsigned char *out_buffer, int *bytes_written) {

  fprintf(stderr, "Got a filled buffer with %d, allocated %d\n",
      buff_header->nFilledLen, buff_header->nAllocLen);

  if (buff_header->nFilledLen > 0) {
    void *buffer_data = out_buffer + *bytes_written;
    *bytes_written += buff_header->nFilledLen;
    if (*bytes_written > OUTPUT_BUFFER_SIZE) {
      fprintf(stderr, "Output buffer not large enough!\n");
      exit(EXIT_FAILURE);
    }
    memcpy(buffer_data, buff_header->pBuffer, buff_header->nFilledLen);
  }

  if (buff_header->nFlags & OMX_BUFFERFLAG_EOS) {
    fprintf(stderr, "Got EOS on output\n");
    return true;
  }

  OMX_ERRORTYPE err = OMX_FillThisBuffer(ilclient_get_handle(component), buff_header);
  if (err != OMX_ErrorNone) {
    fprintf(stderr, "Fill buffer error 0x%X\n", (unsigned int)err);
    exit(EXIT_FAILURE);
  }

  return false;
}


int main(int argc, char *argv[]) {
  ILCLIENT_T *handle = 0;

  bcm_host_init();

  handle = ilclient_init();

  if (!handle) {
    fprintf(stderr, "Failed to initialise ilclient\n");
    exit(EXIT_FAILURE);
  } 

  if (OMX_Init() != OMX_ErrorNone) {
    fprintf(stderr, "Failed to initialise OMX\n");
    exit(EXIT_FAILURE);
  }

  ilclient_set_error_callback(handle, error_callback, NULL);
  ilclient_set_eos_callback(handle, eos_callback, NULL);

  // ***************************************************
  // ********** Initialise component pipeline **********
  // ***************************************************

  // Create image_decode component.
  OMX_HELPER_COMPONENT decode_component;
  create_and_initialise_image_decode_component(handle, &decode_component);
  set_component_input_image_format(&decode_component, OMX_IMAGE_CodingJPEG);

  // Set up resize component...
  OMX_HELPER_COMPONENT resize_component;
  create_and_initialise_resize_component(handle, &resize_component);
  set_resize_params(&resize_component, 1024, 768);

  // Set up encode component...
  OMX_HELPER_COMPONENT encode_component;
  create_and_initialise_image_encode_component(handle, &encode_component);
  set_component_output_image_format(&encode_component, OMX_IMAGE_CodingJPEG);

  // Create and initialise pipeline.
  connect_components(&decode_component, &resize_component);
  connect_components(&resize_component, &encode_component);

  initialise_pipeline(&decode_component);

  // Command-line args.
  char *image_filepath = "/mnt/data/timelapse/current.jpg";
  if (argc >= 2) {
    image_filepath = argv[1];
  }

  fprintf(stderr, "Resizing image %s\n", image_filepath);
  FILE *f = fopen(image_filepath, "rb");
  if (!f) {
    fprintf(stderr, "Can't open image file!\n");
    exit(EXIT_FAILURE);
  }

  fseek(f, 0, SEEK_END);
  int bytes_to_read = ftell(f);
  fseek(f, 0, SEEK_SET);

  prime_component_with_stream(&decode_component, f);
  bytes_to_read -= ftell(f);

  start_pipeline(&decode_component);


  // Allocate output buffer.
  // TODO: realloc if too small...
  unsigned char *out_buffer = calloc(5 * 1024 * 1024, 1);
  if (!out_buffer) {
    fprintf(stderr, "Failed to allocate output buffer!\n");
    exit(EXIT_FAILURE);
  }
  int bytes_written = 0;

  // Write data to image_decode and read from image_decode...
  OMX_BUFFERHEADERTYPE *buff_header;
  while (bytes_to_read) {
    buff_header = ilclient_get_input_buffer(decode_component.component, 320, 1);
    if (buff_header != NULL) {
      read_into_buffer_and_empty(f, decode_component.component, buff_header, &bytes_to_read);
    }

    buff_header = ilclient_get_output_buffer(encode_component.component, 341, 0);
    if (buff_header != NULL) {
      process_output_chunk(encode_component.component, buff_header, out_buffer, &bytes_written);
    }
  }

  while (1) {
    fprintf(stderr, "Getting the last output buffers...\n");
    buff_header = ilclient_get_output_buffer(encode_component.component, 341, 1);
    if (buff_header != NULL) {
      if (process_output_chunk(encode_component.component, buff_header, out_buffer, &bytes_written)) {
        break;
      }
    } else {
      break;
    }
  }

  fprintf(stderr, "Closing file...\n");
  fclose(f);

  // *******************************************
  // ********** Process output buffer **********
  // *******************************************

  fprintf(stderr, "%d\n", bytes_written);

  // Write this frame to stdout.
  fwrite(out_buffer, bytes_written, 1, stdout);
  fflush(stdout);


  exit(EXIT_SUCCESS);
}
