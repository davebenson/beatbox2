
typedef struct _Renderer Renderer;
struct _Renderer
{
  guint text_height;
  guint width;
  guint8 *rolling;
  FILE *fp;
};

void renderer_init     (Renderer *to_init,  guint width);
void renderer_add_line (Renderer *renderer, const guint8 *data);
void renderer_add_text (Renderer *renderer, const guint8 *rgb, guint left, const char *text);


