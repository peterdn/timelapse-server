#include <stdio.h>
#include <stdlib.h>

#include <OMX_Core.h>

#include <bcm_host.h>

#include <ilclient.h>

void error_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
  fprintf(stderr, "OMX error 0x%X\n", (unsigned int)data);
}

void eos_callback(void *userdata, COMPONENT_T *comp, OMX_U32 data) {
  fprintf(stderr, "Got EOS event 0x%X\n", (unsigned int)data);
}

void print_state(OMX_HANDLETYPE *handle) {
  OMX_STATETYPE state;
  OMX_ERRORTYPE err = OMX_GetState(handle, &state);
  if (err != OMX_ErrorNone) {
    fprintf(stderr, "Error getting state\n");
    exit(EXIT_FAILURE);
  }

  switch (state) {
  case OMX_StateLoaded:
    printf("StateLoaded\n");
    break;
  case OMX_StateIdle:
    printf("StateIdle\n");
    break;
  case OMX_StateExecuting:
    printf("StateExecuting\n");
    break;
  case OMX_StatePause:
    printf("StatePause\n");
    break;
  case OMX_StateWaitForResources:
    printf("StateWaitForResources\n");
    break;
  case OMX_StateInvalid:
    printf("StateInvalid\n");
    break;
  default:
    printf("State unknown\n");
    break;
  }
}

void set_image_decoder_input_format(COMPONENT_T *component) {
  printf("Setting image decoder format\n");

  OMX_IMAGE_PARAM_PORTFORMATTYPE image_port_format;
  memset(&image_port_format, 0, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
  image_port_format.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
  image_port_format.nVersion.nVersion =  OMX_VERSION;

  image_port_format.nPortIndex = 320;
  image_port_format.eCompressionFormat = OMX_IMAGE_CodingJPEG;

  OMX_SetParameter(ilclient_get_handle(component), OMX_IndexParamImagePortFormat, &image_port_format);
}

OMX_ERRORTYPE read_into_buffer_and_empty(FILE *f, COMPONENT_T *component,
    OMX_BUFFERHEADERTYPE *buff_header, int *toread) {

  int buff_size = buff_header->nAllocLen;
  int nread = fread(buff_header->pBuffer, 1, buff_size, f);

  printf("Read %d bytes\n", nread);

  buff_header->nFilledLen = nread;
  *toread -= nread;
  if (*toread <= 0) {
    printf("Setting EOS on input\n");
    buff_header->nFlags |= OMX_BUFFERFLAG_EOS;
  }

  OMX_ERRORTYPE err = OMX_EmptyThisBuffer(ilclient_get_handle(component), buff_header);
  if (err != OMX_ErrorNone) {
    fprintf(stderr, "Empty buffer error 0x%X\n", (unsigned int)err);
  }

  return err;
}

OMX_ERRORTYPE save_info_from_filled_buffer(COMPONENT_T *component,
    OMX_BUFFERHEADERTYPE *buff_header) {

  printf("Got a filled buffer with %d, allocated %d\n",
      buff_header->nFilledLen, buff_header->nAllocLen);
  if (buff_header->nFlags & OMX_BUFFERFLAG_EOS) {
    printf("Got EOS on output\n");
    printf("Writing image...\n");
    
    FILE *of = fopen("output.jpg", "wb");
    int nwritten = fwrite(buff_header->pBuffer, buff_header->nFilledLen, 1, of);
    if (nwritten != 1) {
      fprintf(stderr, "Failed to write all data to output file!\n");
      exit(EXIT_FAILURE);
    }
    fclose(of);
    printf("Success!!!!\n");
    exit(EXIT_SUCCESS);
  }

  // TODO: something with the data.

  OMX_ERRORTYPE err = OMX_FillThisBuffer(ilclient_get_handle(component), buff_header);
  if (err != OMX_ErrorNone) {
    fprintf(stderr, "Fill buffer error 0x%X\n", (unsigned int)err);
  }

  return err;
}

void get_output_port_settings(COMPONENT_T *component) {

  printf("Port settings changed!\n");

  // Need to setup the input for the resizer with the output of the decoder.
  OMX_PARAM_PORTDEFINITIONTYPE portdef;
  memset(&portdef, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
  portdef.nVersion.nVersion = OMX_VERSION;
  portdef.nPortIndex = 321;

  OMX_GetParameter(ilclient_get_handle(component), OMX_IndexParamPortDefinition, &portdef);

  unsigned int width = (unsigned int)portdef.format.image.nFrameWidth;
  unsigned int height = (unsigned int)portdef.format.image.nFrameHeight;
  unsigned int stride = (unsigned int)portdef.format.image.nStride;
  unsigned int slice_height = (unsigned int)portdef.format.image.nSliceHeight;

  printf("Width: %d\nHeight: %d\nStride: %d\nSlice height: %d\n", width, height, stride, slice_height);

  printf("Format compression: 0x%X\nColor format: 0x%X\n",
      (unsigned int) portdef.format.image.eCompressionFormat,
      (unsigned int) portdef.format.image.eColorFormat);
}

void set_resize_component_params(COMPONENT_T *component) {
  OMX_PARAM_PORTDEFINITIONTYPE portdef;
  memset(&portdef, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
  portdef.nVersion.nVersion = OMX_VERSION;
  portdef.nPortIndex = 61;
  OMX_GetParameter(ilclient_get_handle(component), OMX_IndexParamPortDefinition, &portdef);
  portdef.format.image.nFrameWidth = 1080;
  portdef.format.image.nFrameHeight = 810;
  portdef.format.image.nStride = 1088;
  portdef.format.image.nSliceHeight = 0;
  portdef.format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;

  OMX_ERRORTYPE err;
  if ((err = OMX_SetParameter(ilclient_get_handle(component),
        OMX_IndexParamPortDefinition, &portdef)) != OMX_ErrorNone) {
    fprintf(stderr, "Error setting resize parameters: 0x%X\n", err);
    exit(EXIT_FAILURE);
  }
}

void create_and_init_component(ILCLIENT_T *handle,
    COMPONENT_T **component, char *component_name) {
  int err = ilclient_create_component(handle, component, component_name,
      ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS | ILCLIENT_ENABLE_OUTPUT_BUFFERS);

  if (err < 0) {
    fprintf(stderr, "Failed to initialise component %s, error code 0x%X\n",
        component_name, err);
    exit(EXIT_FAILURE);
  }

  print_state(ilclient_get_handle(*component));

  // Move component to idle state.
  if ((err = ilclient_change_component_state(*component, OMX_StateIdle)) < 0) {
    fprintf(stderr, "Failed to change component state to idle!\n");
    exit(EXIT_FAILURE);
  }

  print_state(ilclient_get_handle(*component));
}

int main(int argc, char *argv[]) {
  ILCLIENT_T *handle = 0;
  COMPONENT_T *decode_component;

  bcm_host_init();

  char *component_name = "image_decode";
  handle = ilclient_init();

  if (!handle) {
    fprintf(stderr, "Failed to initialise ilclient\n");
    exit(EXIT_FAILURE);
  } else {
    printf("handle = 0x%X\n", (unsigned int)handle);
  }

  if (OMX_Init() != OMX_ErrorNone) {
    fprintf(stderr, "Failed to initialise OMX\n");
    exit(EXIT_FAILURE);
  }

  ilclient_set_error_callback(handle, error_callback, NULL);
  ilclient_set_eos_callback(handle, eos_callback, NULL);

  // Create image_decode component in loaded state with all ports disabled.
  create_and_init_component(handle, &decode_component, "image_decode");

  set_image_decoder_input_format(decode_component);

  // Create input port buffer and enable port.
  ilclient_enable_port_buffers(decode_component, 320, NULL, NULL, NULL);
  ilclient_enable_port(decode_component, 320);

  print_state(ilclient_get_handle(decode_component));


  // Set up resize component...
  COMPONENT_T *resize_component;
  create_and_init_component(handle, &resize_component, "resize");

  // Set up resize parameters.
  set_resize_component_params(resize_component);

  // Enable output port of resize component
  ilclient_enable_port_buffers(resize_component, 61, NULL, NULL, NULL);
  ilclient_enable_port(resize_component, 61);

  print_state(ilclient_get_handle(resize_component));


  // Move image_decode component to executing state.

  int err;
  if ((err = ilclient_change_component_state(decode_component, OMX_StateExecuting)) < 0) {
    fprintf(stderr, "Failed to transition to executing state!\n");
    exit(EXIT_FAILURE);
  }

  print_state(ilclient_get_handle(decode_component));

  // Read the first block so that the component can get the dimensions of the
  // image and call port settings changed on the output port to configure it.
  OMX_BUFFERHEADERTYPE *buff_header = ilclient_get_input_buffer(decode_component, 320, 1);

  if (buff_header == NULL) {
    fprintf(stderr, "Failed to allocate a buffer :-(\n");
    exit(EXIT_FAILURE);
  }

  FILE *f = fopen("/mnt/data/timelapse/current.jpg", "rb");
  if (!f) {
    fprintf(stderr, "Can't open image file!\n");
    exit(EXIT_FAILURE);
  }

  fseek(f, 0, SEEK_END);
  int toread = ftell(f);
  fseek(f, 0, SEEK_SET);
  printf("File size = %d bytes\n", toread);

  read_into_buffer_and_empty(f, decode_component, buff_header, &toread);

  // If all the file has been read in, then we have to re-read the first block -- Broadcom bug?
  if (toread <= 0) {
    printf("Rewinding...\n");
    fseek(f, 0, SEEK_SET);
  }

  // Wait for the first input block to set params for output port.
  ilclient_wait_for_event(decode_component, OMX_EventPortSettingsChanged, 321, 0, 0, 1,
      ILCLIENT_EVENT_ERROR | ILCLIENT_PARAMETER_CHANGED, 10000);

  get_output_port_settings(decode_component);


  // Set up the tunnel between the ports of components A and B.
  TUNNEL_T tunnel;
  set_tunnel(&tunnel, decode_component, 321, resize_component, 60);
  if ((err = ilclient_setup_tunnel(&tunnel, 0, 0)) < 0) {
    fprintf(stderr, "Failed to setup tunnel\n");
    exit(EXIT_FAILURE);
  } else {
    printf("Tunnel has been set up!\n");
  }

  // Enable decode output ports
  ilclient_enable_port(decode_component, 321);

  print_state(ilclient_get_handle(decode_component));

  // Enable resize component input ports
  ilclient_enable_port(resize_component, 60);

  print_state(ilclient_get_handle(resize_component));

  // Set states to executing...
  printf("Setting image_decode to executing...\n");
  if ((err = ilclient_change_component_state(decode_component, OMX_StateExecuting)) < 0) {
    fprintf(stderr, "Couldn't set image_decode to executing: 0x%X\n", err);
    exit(EXIT_FAILURE);
  }
  print_state(ilclient_get_handle(decode_component));

  printf("Setting resize to executing...\n");
  if ((err = ilclient_change_component_state(resize_component, OMX_StateExecuting)) < 0) {
    fprintf(stderr, "Couldn't set resize to executing: 0x%X\n", err);
    exit(EXIT_FAILURE);
  }
  print_state(ilclient_get_handle(resize_component));


  // Write data to image_decode and read from resize...

  while (toread > 0) {
    buff_header = ilclient_get_input_buffer(decode_component, 320, 1);
    if (buff_header != NULL) {
      read_into_buffer_and_empty(f, decode_component, buff_header, &toread);
    }

    buff_header = ilclient_get_output_buffer(resize_component, 61, 0);
    if (buff_header != NULL) {
      save_info_from_filled_buffer(resize_component, buff_header);
    }
  }

  while (1) {
    printf("Getting the last output buffers...\n");
    buff_header = ilclient_get_output_buffer(resize_component, 61, 1);
    if (buff_header != NULL) {
      save_info_from_filled_buffer(resize_component, buff_header);
    } else {
      break;
    }
  }

  fclose(f);
  printf("Closing file...\n");

  exit(EXIT_SUCCESS);
}