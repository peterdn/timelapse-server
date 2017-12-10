#include "component.h"

#include <stdio.h>

static int create_and_initialise_component(OMX_HELPER_COMPONENT *component,
        ILCLIENT_T *ilclient_handle, char *component_name) {
  
  component->next_component = NULL;
  component->ilclient_handle = ilclient_handle;
  component->component_name = component_name;
  
  int err = ilclient_create_component(component->ilclient_handle, 
      &component->component, component->component_name,
      ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS | ILCLIENT_ENABLE_OUTPUT_BUFFERS);

  if (err < 0) {
    fprintf(stderr, "Failed to initialise component %s, error code 0x%X\n",
        component->component_name, err);
    return err;
  }

  // Move component to idle state.
  if ((err = ilclient_change_component_state(component->component, OMX_StateIdle)) < 0) {
    fprintf(stderr, "Failed to change component state to idle!\n");
    return err;
  }

  return err;
}


int create_and_initialise_image_decode_component(
    ILCLIENT_T *ilclient_handle, OMX_HELPER_COMPONENT *component) {
  int err = create_and_initialise_component(component, ilclient_handle, "image_decode");

  component->inport = 320;
  component->outport = 321;
  return err;
}

int create_and_initialise_image_encode_component(
    ILCLIENT_T *ilclient_handle, OMX_HELPER_COMPONENT *component) {

  int err = create_and_initialise_component(component, ilclient_handle, "image_encode");
  component->inport = 340;
  component->outport = 341;
  return err;
}

int create_and_initialise_resize_component(
    ILCLIENT_T *ilclient_handle, OMX_HELPER_COMPONENT *component) {

  int err = create_and_initialise_component(component, ilclient_handle, "resize");
  component->inport = 60;
  component->outport = 61;
  return err;
}


OMX_HELPER_COMPONENT * connect_components(OMX_HELPER_COMPONENT *input_component,
    OMX_HELPER_COMPONENT *output_component) {
  
  // Simply connects up the components in a linked list.
  return input_component->next_component = output_component;
}

int initialise_pipeline(OMX_HELPER_COMPONENT *input_component) {

  // Enable input buffers and input port on the input component.
  ilclient_enable_port_buffers(input_component->component, 
      input_component->inport, NULL, NULL, NULL);
  ilclient_enable_port(input_component->component,
      input_component->inport);

  OMX_HELPER_COMPONENT *output_component = input_component;
  while (output_component->next_component != NULL) {
    output_component = output_component->next_component;
  }

  // Enable output buffers and output port on the output component.
  ilclient_enable_port_buffers(output_component->component, 
      output_component->outport, NULL, NULL, NULL);
  ilclient_enable_port(output_component->component,
      output_component->outport);

  return 0;
}

static void set_component_image_format(OMX_HELPER_COMPONENT *component,
    OMX_IMAGE_CODINGTYPE image_format, int port) {

  OMX_IMAGE_PARAM_PORTFORMATTYPE image_port_format;
  memset(&image_port_format, 0, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
  image_port_format.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
  image_port_format.nVersion.nVersion =  OMX_VERSION;

  image_port_format.nPortIndex = port;
  image_port_format.eCompressionFormat = image_format;

  OMX_SetParameter(ilclient_get_handle(component->component),
      OMX_IndexParamImagePortFormat, &image_port_format);
}

void set_component_input_image_format(OMX_HELPER_COMPONENT *component,
    OMX_IMAGE_CODINGTYPE image_format) {

  set_component_image_format(component, image_format, component->inport);
}

void set_component_output_image_format(OMX_HELPER_COMPONENT *component,
    OMX_IMAGE_CODINGTYPE image_format) {

  set_component_image_format(component, image_format, component->outport);
}

int set_resize_params(OMX_HELPER_COMPONENT *component,
    int width, int height) {

  OMX_PARAM_PORTDEFINITIONTYPE portdef;
  memset(&portdef, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
  portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
  portdef.nVersion.nVersion = OMX_VERSION;
  portdef.nPortIndex = component->outport;

  OMX_GetParameter(ilclient_get_handle(component->component),
    OMX_IndexParamPortDefinition, &portdef);
  portdef.format.image.nFrameWidth = width;
  portdef.format.image.nFrameHeight = height;
  portdef.format.image.nStride = width % 32 ? ((width >> 5) + 1) << 5 : width;
  portdef.format.image.nSliceHeight = 0;
  portdef.format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;

  OMX_ERRORTYPE err;
  if ((err = OMX_SetParameter(ilclient_get_handle(component->component),
        OMX_IndexParamPortDefinition, &portdef)) != OMX_ErrorNone) {
    fprintf(stderr, "Error setting resize parameters: 0x%X\n", err);
    return err;
  }
}

int start_pipeline(OMX_HELPER_COMPONENT *input_component) {
  
  int err = 0;

  // Tunnel components together.
  OMX_HELPER_COMPONENT *component = input_component;
  while (component->next_component != NULL) {
    TUNNEL_T tunnel;
    set_tunnel(&tunnel, component->component, component->outport, 
        component->next_component->component, component->next_component->inport);
    if ((err = ilclient_setup_tunnel(&tunnel, 0, 0)) < 0) {
      fprintf(stderr, "Failed to setup tunnel %s -> %s, error code: 0x%X\n",
          component->component_name, component->next_component->component_name, err);
    }
    component = component->next_component;
  }

  // Enable all ports.
  component = input_component;
  while (component != NULL) {
    ilclient_enable_port(component->component, component->inport);
    ilclient_enable_port(component->component, component->outport);
    component = component->next_component;
  }

  // Move all components to executing state.
  component = input_component;
  while (component != NULL) {
    if ((err = ilclient_change_component_state(component->component, OMX_StateExecuting)) < 0) {
      fprintf(stderr, "Couldn't set component %s to executing: error code 0x%X\n",
          component->component_name, err);
      return err;
    }
    component = component->next_component;
  }

  return err;
}

int prime_component_with_stream(OMX_HELPER_COMPONENT *component, FILE *f) {

  // Move component to executing state.
  int err;
  if ((err = ilclient_change_component_state(component->component, OMX_StateExecuting)) < 0) {
    fprintf(stderr, "Failed to transition component %s to executing state!\n",
        component->component_name);
    return err;
  }

  // Read the first block so that the component can get the dimensions of the
  // image and call port settings changed on the output port to configure it.
  OMX_BUFFERHEADERTYPE *buff_header = ilclient_get_input_buffer(
      component->component, component->inport, 1);

  if (buff_header == NULL) {
    fprintf(stderr, "Failed to allocate a buffer for component %s on port %d\n",
        component->component_name, component->inport);
    return err;
  }

  int nread = fread(buff_header->pBuffer, 1, buff_header->nAllocLen, f);
  buff_header->nFilledLen = nread;
  
  OMX_EmptyThisBuffer(ilclient_get_handle(component->component), buff_header);

  // Wait for the first input block to set params for output port.
  ilclient_wait_for_event(component->component, OMX_EventPortSettingsChanged, component->outport,
      0, 0, 1, ILCLIENT_EVENT_ERROR | ILCLIENT_PARAMETER_CHANGED, 10000);

  return err;
}