/*
 * ui.c — retained tree UI system
 *
 * The user provides:
 *   struct ui_node  g_ui_nodes[]   — flat node array, indexed by user enum
 *   struct ui_style g_ui_styles[]  — flat style array, index 0 = builtin default
 *   uint16_t        g_ui_root      — index of root node
 *
 * Each frame:
 *   ui_solve(g_ui_root, 0, 0, window_w, window_h);
 *   uint32_t n = ui_emit(g_ui_root, ui_instances);
 */

#include <basic/basic.h>
#include <stdio.h>

/* -------------------------------------------------------------------------
 * Internal state
 * ---------------------------------------------------------------------- */

static uint16_t g_hovered = 0;
static uint16_t g_focused = 0;
static float    g_mouse_x = 0;
static float    g_mouse_y = 0;

/* dispatch table — user registers handlers via ui_on_click */
#define UI_HANDLER_MAX 256
struct ui_handler {
    void (*fn)(void *userdata);
    void *userdata;
};
static struct ui_handler g_click_handlers[UI_HANDLER_MAX];
static struct ui_node* g_node_ctx = NULL;

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

static inline struct ui_style *node_style(struct ui_node *node)
{
    uint16_t sid = (node->id &&
                    node->id == g_hovered  &&
                    node->style_hover != 0)
                 ? node->style_hover
                 : node->style;
    return &g_ui_styles[sid];
}

static inline void *binding_resolve(struct ui_binding *b)
{
    if (!b->base) return NULL;
    return (uint8_t *)b->base + b->offset;
}

/* -------------------------------------------------------------------------
 * Pass 1 — fixed and percent sizes (top-down)
 * ---------------------------------------------------------------------- */

static void ui_pass_fixed(uint16_t idx, float parent_w, float parent_h)
{
    (void)parent_w;
    (void)parent_h;

    struct ui_node *node = &g_node_ctx[idx];

    if (UI_IS_FIXED(node->w)) node->solved_w = node->w;
    if (UI_IS_FIXED(node->h)) node->solved_h = node->h;
    /* GROW and HUG resolved in later passes */

    uint16_t child = node->first_child;
    while (child) {
        ui_pass_fixed(child, node->solved_w, node->solved_h);
        child = g_node_ctx[child].next_sibling;
    }
}

/* -------------------------------------------------------------------------
 * Pass 2 — hug sizes (bottom-up)
 * ---------------------------------------------------------------------- */

static void ui_pass_hug(uint16_t idx)
{
    struct ui_node *node = &g_node_ctx[idx];

    /* recurse first — children must be resolved before parent hug */
    uint16_t child = node->first_child;
    while (child) {
        ui_pass_hug(child);
        child = g_node_ctx[child].next_sibling;
    }

    if (node->kind == UI_NODE_TEXT && node->text) {
        struct ui_style *style = &g_ui_styles[node->style];
        float scale = style->text_scale > 0 ? style->text_scale : 1.0f;
        uint32_t len = 0;
        /******************/
        /******* TODO *****/
        /******************/
        const float glyph_w = 6.0f;
        const float glyph_h = 13.0f;
        for (const char *p = node->text; *p; p++) len++;
        if (UI_IS_HUG(node->w)) node->solved_w = len * glyph_w * scale;
        if (UI_IS_HUG(node->h)) node->solved_h = glyph_h * scale;
        return;
    }

    if (!UI_IS_HUG(node->w) && !UI_IS_HUG(node->h)) return;

    float content_w = 0, content_h = 0;
    uint32_t i = 0;
    child = node->first_child;
    while (child) {
        struct ui_node *c = &g_node_ctx[child];
        if (node->direction == UI_ROW) {
            content_w += c->solved_w + (i > 0 ? node->gap : 0);
            if (c->solved_h > content_h) content_h = c->solved_h;
        } else {
            content_h += c->solved_h + (i > 0 ? node->gap : 0);
            if (c->solved_w > content_w) content_w = c->solved_w;
        }
        i++;
        child = c->next_sibling;
    }

    content_w += node->pad * 2;
    content_h += node->pad * 2;

    if (UI_IS_HUG(node->w)) node->solved_w = content_w;
    if (UI_IS_HUG(node->h)) node->solved_h = content_h;
}

/* -------------------------------------------------------------------------
 * Pass 3 — grow sizes (top-down)
 * ---------------------------------------------------------------------- */

static void ui_pass_grow(uint16_t idx)
{
    struct ui_node *node = &g_node_ctx[idx];
    if (!node->first_child) return;

    float available_w = node->solved_w - node->pad * 2;
    float available_h = node->solved_h - node->pad * 2;
    uint32_t grow_count = 0;
    uint32_t i = 0;

    uint16_t child = node->first_child;
    while (child) {
        struct ui_node *c = &g_node_ctx[child];
        if (node->direction == UI_ROW) {
            if (!UI_IS_GROW(c->w)) available_w -= c->solved_w;
            else grow_count++;
            if (i > 0) available_w -= node->gap;
        } else {
            if (!UI_IS_GROW(c->h)) available_h -= c->solved_h;
            else grow_count++;
            if (i > 0) available_h -= node->gap;
        }
        i++;
        child = c->next_sibling;
    }

    float grow_w = grow_count ? available_w / grow_count : 0;
    float grow_h = grow_count ? available_h / grow_count : 0;

    child = node->first_child;
    while (child) {
        struct ui_node *c = &g_node_ctx[child];
        if (node->direction == UI_ROW) {
            if (UI_IS_GROW(c->w)) c->solved_w = grow_w;
            if (UI_IS_GROW(c->h)) c->solved_h = node->solved_h - node->pad * 2;
        } else {
            if (UI_IS_GROW(c->h)) c->solved_h = grow_h;
            if (UI_IS_GROW(c->w)) c->solved_w = node->solved_w - node->pad * 2;
        }
        ui_pass_grow(child);
        child = c->next_sibling;
    }
}

/* -------------------------------------------------------------------------
 * Pass 4 — positions (top-down)
 * ---------------------------------------------------------------------- */

static void ui_pass_position(uint16_t idx, float x, float y)
{
    struct ui_node *node = &g_node_ctx[idx];
    node->solved_x = x;
    node->solved_y = y;

    if (!node->first_child) return;

    float available_main  = (node->direction == UI_ROW
                            ? node->solved_w : node->solved_h) - node->pad * 2;
    float available_cross = (node->direction == UI_ROW
                            ? node->solved_h : node->solved_w) - node->pad * 2;

    /* compute total children size along main axis for alignment */
    float total_main = 0;
    uint32_t child_count = 0;
    uint16_t child = node->first_child;
    while (child) {
        struct ui_node *c = &g_node_ctx[child];
        total_main += node->direction == UI_ROW ? c->solved_w : c->solved_h;
        child_count++;
        child = c->next_sibling;
    }
    if (child_count > 1) total_main += node->gap * (child_count - 1);

    /* main axis start offset */
    float main_offset = 0;
    switch (node->align_main) {
        case UI_ALIGN_START:  main_offset = 0; break;
        case UI_ALIGN_CENTER: main_offset = (available_main - total_main) / 2.0f; break;
        case UI_ALIGN_END:    main_offset = available_main - total_main; break;
    }

    float cursor_x = x + node->pad + (node->direction == UI_ROW    ? main_offset : 0);
    float cursor_y = y + node->pad + (node->direction == UI_COLUMN  ? main_offset : 0);

    child = node->first_child;
    while (child) {
        struct ui_node *c = &g_node_ctx[child];

        /* cross axis offset per child */
        float cross_size   = node->direction == UI_ROW ? c->solved_h : c->solved_w;
        float cross_offset = 0;
        switch (node->align_cross) {
            case UI_ALIGN_START:  cross_offset = 0; break;
            case UI_ALIGN_CENTER: cross_offset = (available_cross - cross_size) / 2.0f; break;
            case UI_ALIGN_END:    cross_offset = available_cross - cross_size; break;
        }

        float child_x = cursor_x + (node->direction == UI_COLUMN ? cross_offset : 0);
        float child_y = cursor_y + (node->direction == UI_ROW    ? cross_offset : 0);

        ui_pass_position(child, child_x, child_y);

        if (node->direction == UI_ROW) cursor_x += c->solved_w + node->gap;
        else                           cursor_y += c->solved_h + node->gap;

        child = c->next_sibling;
    }

    /* store content size for scroll clamping */
    node->content_h = (cursor_y - y) + node->pad;
}

/* -------------------------------------------------------------------------
 * Public: ui_solve
 * ---------------------------------------------------------------------- */

void ui_solve(uint16_t root, float x, float y, float w, float h)
{
    g_node_ctx = g_ui_nodes;
    g_node_ctx[root].solved_w = w;
    g_node_ctx[root].solved_h = h;
    ui_pass_fixed(root, w, h);
    ui_pass_hug(root);
    ui_pass_grow(root);
    ui_pass_position(root, x, y);
}

/* -------------------------------------------------------------------------
 * Emit helpers
 * ---------------------------------------------------------------------- */

/* forward declared — provided by the application */
extern uint32_t draw_text(struct ui_instance *dst, const char *text,
        float scale, float x, float y,
        float r, float g, float b, float a,
        uint32_t tex_index);
extern uint32_t g_atlas_texture;

static uint32_t ui_emit_node(uint16_t idx, struct ui_instance *dst)
{
    struct ui_node  *node  = &g_node_ctx[idx];
    struct ui_style *style = node_style(node);
    uint32_t n = 0;

    float r, g, b, a;

    switch (node->kind) {

        case UI_NODE_CONTAINER:
        case UI_NODE_RECT: {
            rgba_unpack(style->background, &r, &g, &b, &a);
            if (a > 0.0f) {
                dst[n++] = (struct ui_instance){
                    .x = node->solved_x, .y = node->solved_y,
                    .w = node->solved_w, .h = node->solved_h,
                    .r = r, .g = g, .b = b, .a = a,
                    .tex_index = INVALID_TEXTURE,
                };
            }
            break;
        }

        case UI_NODE_IMAGE: {
            uint32_t tex = style->tex_index != INVALID_TEXTURE
                         ? style->tex_index
                         : node->tex_index;
            dst[n++] = (struct ui_instance){
                .x = node->solved_x, .y = node->solved_y,
                .w = node->solved_w, .h = node->solved_h,
                .r = 1, .g = 1, .b = 1, .a = 1,
                .u0 = 0, .v0 = 0, .u1 = 1, .v1 = 1,
                .tex_index = tex,
            };
            break;
        }

        case UI_NODE_TEXT: {
            /* resolve text — binding overrides static */
            const char *text = node->text;
            static char fmt_buf[256];
            if (node->binding.kind == UI_BIND_INT && node->binding.base) {
                int val = *(int *)binding_resolve(&node->binding);
                snprintf(fmt_buf, sizeof(fmt_buf), "%d", val);
                text = fmt_buf;
            } else if (node->binding.kind == UI_BIND_FLOAT && node->binding.base) {
                float val = *(float *)binding_resolve(&node->binding);
                snprintf(fmt_buf, sizeof(fmt_buf), "%.2f", val);
                text = fmt_buf;
            } else if (node->binding.kind == UI_BIND_STRING && node->binding.base) {
                text = *(const char **)binding_resolve(&node->binding);
            }

            if (text) {
                rgba_unpack(style->text, &r, &g, &b, &a);
                float scale = style->text_scale > 0 ? style->text_scale : 1.0f;
                /* draw_text expects y at baseline */
                n += draw_text(dst + n, text, scale,
                        node->solved_x,
                        node->solved_y,
                        r, g, b, a, g_atlas_texture);
            }
            break;
        }

        case UI_NODE_LIST: {
            uint32_t count = *node->list_count;
            for (uint32_t i = 0; i < count; i++) {
                float item_y = node->solved_y + node->pad
                    + i * (node->list_item_h + node->gap)
                    - node->scroll_y;
                if (item_y + node->list_item_h < node->solved_y) continue;
                if (item_y > node->solved_y + node->solved_h)    break;

                // patch template bindings to point at row i
                void *row_data = (uint8_t *)node->list_data + i * node->list_data_stride;
                for (uint32_t t = 0; t < node->row_template_count; t++) {
                    if (node->row_template[t].binding.kind != UI_BIND_NONE)
                        node->row_template[t].binding.base = row_data;
                }

                // solve and emit template
                node->row_template[0].solved_x = node->solved_x + node->pad;
                node->row_template[0].solved_y = item_y;
                node->row_template[0].solved_w = node->solved_w - node->pad * 2;
                node->row_template[0].solved_h = node->list_item_h;
                struct ui_node *saved = g_node_ctx;
                g_node_ctx = node->row_template;
                ui_pass_grow(0); // relative to template root
                ui_pass_position(0, node->row_template[0].solved_x,
                        node->row_template[0].solved_y);
                n += ui_emit_node(0, dst + n); // emit template tree
                g_node_ctx = saved;
            }
            break;
        }
    }

    /* recurse children */
    uint16_t child = node->first_child;
    while (child) {
        n += ui_emit_node(child, dst + n);
        child = g_node_ctx[child].next_sibling;
    }

    return n;
}

uint32_t ui_emit(uint16_t root, struct ui_instance *dst)
{
    return ui_emit_node(root, dst);
}

/* -------------------------------------------------------------------------
 * Hit testing
 * ---------------------------------------------------------------------- */

static uint16_t ui_hit_test(uint16_t idx, float mx, float my)
{
    if (!g_node_ctx) return 0;
    struct ui_node *node = &g_node_ctx[idx];

    /* depth first, reverse sibling order — last child is drawn on top */
    uint16_t result = 0;
    uint16_t child  = node->first_child;
    while (child) {
        uint16_t hit = ui_hit_test(child, mx, my);
        if (hit) result = hit;
        child = g_node_ctx[child].next_sibling;
    }
    if (result) return result;

    if (node->id
        && mx >= node->solved_x && mx < node->solved_x + node->solved_w
        && my >= node->solved_y && my < node->solved_y + node->solved_h) {
        return node->id;
    }

    return 0;
}

/* -------------------------------------------------------------------------
 * Scroll
 * ---------------------------------------------------------------------- */

static void ui_scroll_at(uint16_t idx, float mx, float my, float dy)
{
    struct ui_node *node = &g_node_ctx[idx];

    if (node->scrollable
        && mx >= node->solved_x && mx < node->solved_x + node->solved_w
        && my >= node->solved_y && my < node->solved_y + node->solved_h)
    {
        node->scroll_y += dy;
        if (node->scroll_y < 0) node->scroll_y = 0;
        float max_scroll = node->content_h - node->solved_h;
        if (max_scroll < 0) max_scroll = 0;
        if (node->scroll_y > max_scroll) node->scroll_y = max_scroll;
        return;
    }

    uint16_t child = node->first_child;
    while (child) {
        ui_scroll_at(child, mx, my, dy);
        child = g_node_ctx[child].next_sibling;
    }
}

/* -------------------------------------------------------------------------
 * Public: ui_event
 * ---------------------------------------------------------------------- */

void ui_event(struct ui_event *e)
{
    switch (e->kind) {
        case UI_EVENT_MOUSE_MOVE:
            g_mouse_x = (float)e->mouse_x;
            g_mouse_y = (float)e->mouse_y;
            g_hovered = ui_hit_test(g_ui_root, g_mouse_x, g_mouse_y);
            break;

        case UI_EVENT_MOUSE_DOWN:
            g_focused = g_hovered;
            if (g_hovered > 0 && g_hovered < UI_HANDLER_MAX) {
                struct ui_handler *h = &g_click_handlers[g_hovered];
                if (h->fn) h->fn(h->userdata);
            }
            break;

        case UI_EVENT_SCROLL:
            ui_scroll_at(g_ui_root, g_mouse_x, g_mouse_y, e->scroll_dy);
            break;
        default:
            break;
    }
}

/* -------------------------------------------------------------------------
 * Public: ui_on_click
 * ---------------------------------------------------------------------- */

void ui_on_click(uint16_t id, void (*fn)(void *userdata), void *userdata)
{
    if (id >= UI_HANDLER_MAX) return;
    g_click_handlers[id].fn       = fn;
    g_click_handlers[id].userdata = userdata;
}

/* -------------------------------------------------------------------------
 * Public: ui_hovered / ui_focused
 * ---------------------------------------------------------------------- */

uint16_t ui_hovered(void) { return g_hovered; }
uint16_t ui_focused(void) { return g_focused; }
