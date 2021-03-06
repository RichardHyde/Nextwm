 /* snapwm.c
 *
 *  Started from catwm 31/12/10
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 */

#define _BSD_SOURCE
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
//#include <X11/keysym.h>
/* For a multimedia keyboard */
#include <X11/XF86keysym.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlocale.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/Xrandr.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
// #include <signal.h>
#include <sys/wait.h>
#include <string.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLEANMASK(mask) (mask & ~(numlockmask | LockMask))
#define TABLENGTH(X)    (sizeof(X)/sizeof(*X))

typedef union {
    char *com[15];
    int i;
} Arg;

// Structs
typedef struct {
    unsigned int mod;
    char * keysym;
    void (*myfunction)(const Arg arg);
    Arg arg;
} key;

typedef struct client client;
struct client{
    // Prev and next client
    client *next;
    client *prev;

    // The window
    Window win;
    unsigned int x, y, width, height, order;
};

typedef struct desktop desktop;
struct desktop{
    unsigned int master_size, screen;
    unsigned int mode, growth, numwins, nmaster, showbar;
    unsigned int x, y, w, h;
    client *head, *current, *transient, *focus;
};

typedef struct {
    int cd;
} MonitorView;

typedef struct {
    const char *class;
    unsigned int preferredd;
    unsigned int followwin;
} Convenience;

typedef struct {
    const char *class;
    int x, y, width, height;
} Positional;

typedef struct {
    XFontStruct *font;
    XFontSet fontset;
    int height, width;
    unsigned int fh; /* Y coordinate to draw characters */
    int ascent, descent;
} Iammanyfonts;

typedef struct {
    Window sb_win;
    char *label;
    unsigned int width, labelwidth;
} Barwin;

typedef struct {
    char *modename;
    unsigned long barcolor, wincolor, textcolor;
    GC gc;
} Theme;

typedef struct {
    char *name;
    char *list[15];
} Commands;

// Functions
static void add_window(Window w, int tw, client *cl);
static void buttonpress(XEvent *e);
static void buttonrelease(XEvent *e);
static void change_desktop(const Arg arg);
static int check_dock(Window w);
static void client_to_desktop(const Arg arg);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static void destroynotify(XEvent *e);
static void draw_desk(Window win, unsigned int barcolor, unsigned int gc, unsigned int x, char *string, unsigned int len);
static void draw_text(Window win, unsigned int gc, unsigned int x, char *string, unsigned int len);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void follow_client_to_desktop(const Arg arg);
static unsigned long getcolor(const char* color);
static void get_font();
static char *getwindowname(Window win);
static void grabkeys();
static void init_desks();
static void init_start();
static void keypress(XEvent *e);
static void kill_client();
static void kill_client_now(Window w);
static void last_desktop();
static void leavenotify(XEvent *e);
static void logger(const char* e);
static void mapbar();
static void maprequest(XEvent *e);
static void motionnotify(XEvent *e);
static void more_master(const Arg arg);
static void move_down(const Arg arg);
static void move_up(const Arg arg);
static void move_right(const Arg arg);
static void move_left(const Arg arg);
static void next_win();
static void plugnplay(XEvent *e);
static void prev_win();
static void propertynotify(XEvent *e);
static void quit();
static void remove_client(client *cl, unsigned int dr, unsigned int tw);
static void read_apps_file();
static void read_keys_file();
static void read_rcfile();
static void resize_master(const Arg arg);
static void resize_stack(const Arg arg);
static void rotate_desktop(const Arg arg);
static void rotate_mode(const Arg arg);
static void save_desktop(int i);
static void select_desktop(int i);
static void setbaralpha();
static void setup();
static void setup_status_bar();
static void set_defaults();
static void sigchld(int unused);
static void spawn(const Arg arg);
static void start();
static void status_bar();
static void status_text(char* sb_text);
static void swap_master();
static void switch_mode(const Arg arg);
static void terminate(const Arg arg);
static void tile();
static void toggle_bar();
static void unmapbar();
static void unmapnotify(XEvent *e);    // Thunderbird's write window just unmaps...
static void update_bar();
static void update_config();
static void update_current();
static void update_output(unsigned int messg);
static void warp_pointer();
static unsigned int wc_size(char *string);
static int get_value();

// Include configuration file
#include "config.h"

// Variable
static Display *dis;
static unsigned int attachaside, bdw, bool_quit, clicktofocus, current_desktop, doresize, dowarp, cstack;
static unsigned int screen, followmouse, mode, msize, previous_desktop, DESKTOPS, STATUS_BAR, numwins;
static unsigned int auto_mode, auto_num, shutting_down, default_desk;
static int num_screens, growth, sh, sw, master_size, nmaster, randr_ev;
static unsigned int sb_desks;        // width of the desktop switcher
static unsigned int sb_height, sb_width, screen, show_bar, has_bar, wnamebg, barmon, barmonchange, lessbar;
static unsigned int showopen;        // whether the desktop switcher shows number of open windows
static unsigned int topbar, top_stack, windownamelength, keycount, cmdcount, dtcount, pcount, LA_WINDOWNAME;
static int ufalpha, baralpha;
static unsigned long opacity, baropacity;
static int xerror(Display *dis, XErrorEvent *ee);
static int (*xerrorxlib)(Display *, XErrorEvent *);
unsigned int numlockmask, resizemovekey;        /* dynamic key lock mask */
static Window root;
static Window sb_area;
static client *head, *current, *transient, *focus;
static char font_list[256], buffer[256], dummy[256];
static char RC_FILE[100], KEY_FILE[100], APPS_FILE[100];
static Atom alphaatom, wm_delete_window, protos, *protocols, dockatom, typeatom;
static XWindowAttributes attr;
static XButtonEvent starter;

// Desktop array
static desktop desktops[12];
static MonitorView view[5];
static Barwin sb_bar[12];
static Theme theme[10];
static Iammanyfonts font;
static key keys[80];
static Commands cmds[50];
static Convenience convenience[40];
static Positional positional[40];
#include "bar.c"
#include "readrc.c"
#include "readkeysapps.c"

#include "events.c"
// Events array
static void (*events[LASTEvent])(XEvent *e) = {
    [Expose] = expose,
    [KeyPress] = keypress,
    [MapRequest] = maprequest,
    [EnterNotify] = enternotify,
    [LeaveNotify] = leavenotify,
    [UnmapNotify] = unmapnotify,
    [ButtonPress] = buttonpress,
    [MotionNotify] = motionnotify,
    [ButtonRelease] = buttonrelease,
    [DestroyNotify] = destroynotify,
    [PropertyNotify] = propertynotify,
    [ConfigureNotify] = configurenotify,
    [ConfigureRequest] = configurerequest
};

/* ***************************** Window Management ******************************* */
void add_window(Window w, int tw, client *cl) {
    client *c,*t, *dummy;

    if(cl != NULL) c = cl;
    else {
        if(!(c = (client *)calloc(1,sizeof(client)))) {
            logger("\033[0;31mError calloc!");
            exit(1);
        }
        XGetWindowAttributes(dis, w, &attr);
        c->x = attr.x;
        if(topbar == 0 && attr.y < sb_height+4+bdw) c->y = sb_height+4+bdw;
        else c->y = attr.y;
        c->width = attr.width;
        c->height = attr.height;
    }

    c->win = w; c->order = 0;
    dummy = (tw == 1) ? transient : head;

    if(dummy == NULL) {
        c->next = NULL; c->prev = NULL;
        dummy = c;
    } else {
        for(t=dummy;t;t=t->next)
            ++t->order;
        if(attachaside == 0) {
            if(top_stack == 0) {
                c->next = dummy->next; c->prev = dummy;
                dummy->next = c;
            } else {
                for(t=dummy;t->next;t=t->next); // Start at the last in the stack
                t->next = c; c->next = NULL;
                c->prev = t;
            }
        } else {
            c->prev = NULL; c->next = dummy;
            c->next->prev = c;
            dummy = c;
        }
    }

    focus = c;
    if(tw == 1) {
        transient = dummy;
        save_desktop(current_desktop);
        return;
    } else head = dummy;
    current = c;
    numwins += 1;
    if(growth > 0) growth = growth*(numwins-1)/numwins;
    else growth = 0;
    save_desktop(current_desktop);
    
    if(mode == 4 && auto_num > 0 && numwins >= auto_num)
            mode = auto_mode;

    // for folow mouse and statusbar updates
    if(followmouse == 0 && STATUS_BAR == 0)
        XSelectInput(dis, c->win, PointerMotionMask|PropertyChangeMask);
    else if(followmouse == 0)
        XSelectInput(dis, c->win, PointerMotionMask);
    else if(STATUS_BAR == 0)
        XSelectInput(dis, c->win, PropertyChangeMask);
}

void remove_client(client *cl, unsigned int dr, unsigned int tw) {
    client *t, *dummy;

    dummy = (tw == 1) ? transient : head;
    if(cl->prev == NULL && cl->next == NULL) {
        dummy = NULL;
    } else if(cl->prev == NULL) {
        dummy = cl->next;
        cl->next->prev = NULL;
    } else if(cl->next == NULL) {
        cl->prev->next = NULL;
    } else {
        cl->prev->next = cl->next;
        cl->next->prev = cl->prev;
    }
    
    XUnmapWindow(dis, cl->win);
    if(tw == 1) {
        transient = focus = dummy;
        if(dr == 0) free(cl);
        if(focus == NULL) focus = current;
        save_desktop(current_desktop);
        return;
    } else {
        head = dummy;
        XUngrabButton(dis, AnyButton, AnyModifier, cl->win);
        numwins -= 1;
        if(head != NULL) {
            for(t=head;t;t=t->next) {
                if(t->order > cl->order) --t->order;
                if(t->order == 0) current = t;
            }
        } else current = NULL;
        focus = current;
        if(dr == 0) free(cl);
        if((numwins - nmaster) < 3) growth = 0;
        save_desktop(current_desktop);
        if(mode != 4) tile();
        return;
    }
}

void next_win() {
    if(numwins < 2) {
        if(transient == NULL) return;
        else if(numwins == 0 && transient->next == NULL) return;
    }
    Window w = current->win; client *c;

    if(transient == NULL) {
        current = (current->next == NULL) ? head : current->next;
        focus = current;
    } else {
        if(current->next == NULL) {
            unsigned int set_focus = 0;
            for(c=transient;c;c=c->next) {
                if(c == focus) {
                    if(c->next != NULL) focus = c->next;
                    else current = focus = head;
                    set_focus = 1;
                    break;
                }
            }
            if(set_focus == 0) focus = transient;
        } else {
            current = (focus == current) ? current->next:current;
            focus = current;
        }
    }

    save_desktop(current_desktop);
    if( current == focus && mode == 1) {
        tile();
        XUnmapWindow(dis, w);
    }
    update_current();
}

void prev_win() {
    if(numwins < 2) {
        if(transient == NULL) return;
        else if(numwins == 0 && transient->next == NULL) return;
    }
    client *c; Window w = current->win;

    if(transient == NULL) {
        if(current->prev == NULL) for(c=head;c->next;c=c->next);
        else c = current->prev;
        current = focus = c;
    } else {
        if(current->prev == NULL) {
            unsigned int set_focus = 0;
            for(c=transient;c;c=c->next) {
                if(c == focus) {
                    if(c->next == NULL) {
                        for(c=head;c->next;c=c->next);
                        current = focus = c;
                    } else focus = c->next;
                    set_focus = 1;
                }
            }
            if(set_focus == 0) focus = transient;
        } else {
            current = (focus == current) ? current->prev:current;
            focus = current;
        }
    }
    save_desktop(current_desktop);
    if(mode == 1 && current == focus) {
        tile();
        XUnmapWindow(dis, w);
    }
    update_current();
}

void move_down(const Arg arg) {
    if(mode == 4 && current != NULL) {
        current->y += arg.i;
        XMoveResizeWindow(dis,current->win,desktops[current_desktop].x+current->x,current->y,current->width,current->height);
        return;
    }
    if(current == NULL || current->next == NULL || current->win == head->win || current->prev == NULL)
        return;

    Window tmp = current->win;
    current->win = current->next->win;
    current->next->win = tmp;
    //keep the moved window activated
    next_win();
    update_current();
    save_desktop(current_desktop);
    tile();
}

void move_up(const Arg arg) {
    if(mode == 4 && current != NULL) {
        current->y += arg.i;
        XMoveResizeWindow(dis,current->win,desktops[current_desktop].x+current->x,current->y,current->width,current->height);
        return;
    }
    if(current == NULL || current->prev == head || current->win == head->win)
        return;

    Window tmp = current->win;
    current->win = current->prev->win;
    current->prev->win = tmp;
    prev_win();
    save_desktop(current_desktop);
    tile();
}

void move_left(const Arg arg) {
    if(mode == 4 && current != NULL) {
        current->x += arg.i;
        XMoveResizeWindow(dis,current->win,desktops[current_desktop].x+current->x,current->y,current->width,current->height);
    }
}

void move_right(const Arg arg) {
    move_left(arg);
}

void swap_master() {
    Window tmp;

    if(numwins < 2 || mode == 1) return;
    if(current == head) {
        tmp = head->next->win;
        head->next->win = head->win;
        head->win = tmp;
    } else {
        tmp = head->win;
        head->win = current->win;
        current->win = tmp;
        current = focus = head;
    }
    save_desktop(current_desktop);
    tile();
    update_current();
}

/* **************************** Desktop Management ************************************* */

void change_desktop(const Arg arg) {
    if(arg.i >= DESKTOPS || arg.i == current_desktop) return;
    client *c;

    int next_view = desktops[arg.i].screen;
    if(next_view != desktops[current_desktop].screen && dowarp == 0) {
        XWarpPointer(dis, None, root, 0, 0, 0, 0,
          desktops[arg.i].x+(desktops[arg.i].w/2),
            desktops[arg.i].y+(desktops[arg.i].h/2));
    }

    // Save current "properties"
    save_desktop(current_desktop);
    previous_desktop = current_desktop;

    // Take "properties" from the new desktop
    select_desktop(arg.i);
    if(next_view == barmon && has_bar == 1 && show_bar == 0) mapbar();
    if(next_view == barmon && has_bar == 0 && show_bar == 1) unmapbar();

    // Map all windows
    if(head != NULL) {
        if(mode != 1)
            for(c=head;c;c=c->next)
                XMapWindow(dis,c->win);
        tile();
    }
    if(transient != NULL)
        for(c=transient;c;c=c->next)
            XMapWindow(dis,c->win);

    select_desktop(previous_desktop);
    if(arg.i != view[next_view].cd) {
        select_desktop(view[next_view].cd);
        // Unmap all window
        if(transient != NULL)
            for(c=transient;c;c=c->next)
                XUnmapWindow(dis,c->win);

        if(head != NULL)
            for(c=head;c;c=c->next)
                XUnmapWindow(dis,c->win);
    }

    select_desktop(arg.i);
    view[next_view].cd = current_desktop;
    update_current();
    if(STATUS_BAR == 0) update_bar();
}

void last_desktop() {
    Arg a = {.i = previous_desktop};
    change_desktop(a);
}

void rotate_desktop(const Arg arg) {
    Arg a = {.i = (current_desktop + DESKTOPS + arg.i) % DESKTOPS};
     change_desktop(a);
}

void rotate_mode(const Arg arg) {
    Arg a = {.i = (mode + 5 + arg.i) % 5};
     switch_mode(a);
}

void follow_client_to_desktop(const Arg arg) {
    if(focus == NULL || arg.i == current_desktop || arg.i >= DESKTOPS) return;
    client_to_desktop(arg);
    change_desktop(arg);
}

void client_to_desktop(const Arg arg) {
    if(focus == NULL || arg.i == current_desktop || arg.i >= DESKTOPS) return;

    client *tmp = focus, *c;
    unsigned int tmp2 = current_desktop, j, cd = desktops[current_desktop].screen;
    unsigned int tr = 0;

    if(transient != NULL)
        for(c=transient;c;c=c->next)
            if(c == focus) tr = 1;
    // Remove client from current desktop
    remove_client(tmp, 1, tr);

    // Add client to desktop
    select_desktop(arg.i);
    add_window(tmp->win, tr, tmp);
    save_desktop(arg.i);

    for(j=cd;j<cd+num_screens;++j) {
        if(view[j%num_screens].cd == arg.i) {
            tile();
            XMapWindow(dis, current->win);
        }
    }
    select_desktop(tmp2);
    update_current();

    if(STATUS_BAR == 0) update_bar();
}

void save_desktop(int i) {
    desktops[i].master_size = master_size;
    desktops[i].nmaster = nmaster;
    desktops[i].mode = mode;
    desktops[i].growth = growth;
    desktops[i].showbar = show_bar;
    desktops[i].head = head;
    desktops[i].current = current;
    desktops[i].transient = transient;
    desktops[i].focus = focus;
    desktops[i].numwins = numwins;
}

void select_desktop(int i) {
    master_size = desktops[i].master_size;
    nmaster = desktops[i].nmaster;
    mode = desktops[i].mode;
    growth = desktops[i].growth;
    show_bar = desktops[i].showbar;
    head = desktops[i].head;
    current = desktops[i].current;
    transient = desktops[i].transient;
    focus = desktops[i].focus;
    numwins = desktops[i].numwins;
    current_desktop = i;
    sw = desktops[i].w;
    sh = desktops[i].h;
}

void more_master (const Arg arg) {
    if(arg.i > 0) {
        if((numwins < 3) || (nmaster == (numwins-2))) return;
        nmaster += 1;
    } else {
        if(nmaster == 0) return;
        nmaster -= 1;
    }
    save_desktop(current_desktop);
    tile();
}

void tile() {
    if(head == NULL) return;
    client *c, *d=NULL;
    unsigned int x = 0, xpos = 0, ypos=0, wdt = 0, msw, ssw, ncols = 2, nrows = 1;
    int ht = 0, y, n = 0, nm = (numwins < 3) ? 0: (numwins-2 < nmaster) ? (numwins-2):nmaster;
    int scrx = desktops[current_desktop].x;
    int scry = desktops[current_desktop].y;

    // For a top bar
    y = (STATUS_BAR == 0 && topbar == 0 && show_bar == 0) ? sb_height+4 : 0; ypos = y;

    // If only one window
    if(mode != 4 && head->next == NULL) {
        XMoveResizeWindow(dis,head->win,scrx,scry+y,sw+bdw,sh+bdw);
        if(mode == 1) XMapWindow(dis, current->win);
    } else {
        switch(mode) {
            case 0: /* Vertical */
            	// Master window
            	if(nm < 1)
                    XMoveResizeWindow(dis,head->win,scrx,scry+y,master_size - bdw,sh - bdw);
                else {
                    for(d=head;d;d=d->next) {
                        XMoveResizeWindow(dis,d->win,scrx,scry+ypos,master_size - bdw,sh/(nm+1) - bdw);
                        if(x == nm) break;
                        ypos += sh/(nm+1); ++x;
                    }
                }

                // Stack
                if(d == NULL) d = head;
                n = numwins - (nm+1);
                XMoveResizeWindow(dis,d->next->win,scrx+master_size,scry+y,sw-master_size-bdw,(sh/n)+growth - bdw);
                y += (sh/n)+growth;
                for(c=d->next->next;c;c=c->next) {
                    XMoveResizeWindow(dis,c->win,scrx+master_size,scry+y,sw-master_size-bdw,(sh/n)-(growth/(n-1)) - bdw);
                    y += (sh/n)-(growth/(n-1));
                }
                break;
            case 1: /* Fullscreen */
                XMoveResizeWindow(dis,current->win,scrx,scry+y,sw+bdw,sh+bdw);
                XMapWindow(dis, current->win);
                break;
            case 2: /* Horizontal */
            	// Master window
            	if(nm < 1)
                    XMoveResizeWindow(dis,head->win,scrx+xpos,scry+ypos,sw-bdw,master_size-bdw);
                else {
                    for(d=head;d;d=d->next) {
                        XMoveResizeWindow(dis,d->win,scrx+xpos,scry+ypos,sw/(nm+1)-bdw,master_size-bdw);
                        if(x == nm) break;
                        xpos += sw/(nm+1); ++x;
                    }
                }

                // Stack
                if(d == NULL) d = head;
                n = numwins - (nm+1);
                XMoveResizeWindow(dis,d->next->win,scrx,scry+y+master_size,(sw/n)+growth-bdw,sh-master_size-bdw);
                msw = (sw/n)+growth;
                for(c=d->next->next;c;c=c->next) {
                    XMoveResizeWindow(dis,c->win,scrx+msw,scry+y+master_size,(sw/n)-(growth/(n-1)) - bdw,sh-master_size-bdw);
                    msw += (sw/n)-(growth/(n-1));
                }
                break;
            case 3: { // Grid
                if(numwins < 5) {
                    for(c=head;c;c=c->next) {
                        ++n;
                        if(numwins > 2) {
                            if((n == 1) || (n == 2))
                                ht = (sh/2) + growth - bdw;
                            if(n > 2)
                                ht = (sh/2) - growth - bdw;
                        } else ht = sh - bdw;
                        if((n == 1) || (n == 3)) {
                            xpos = 0;
                            wdt = master_size - bdw;
                        }
                        if((n == 2) || (n == 4)) {
                            xpos = master_size;
                            wdt = (sw - master_size) - bdw;
                        }
                        if(n == 3)
                            ypos += (sh/2) + growth;
                        if((n == numwins) && (n == 3))
                            wdt = sw - bdw;
                        XMoveResizeWindow(dis,c->win,scrx+xpos,scry+ypos,wdt,ht);
                    }
                } else {
                    x = numwins;
                    for(xpos=0;xpos<=x;++xpos) {
                        if(xpos == 3 || xpos == 7 || xpos == 10 || xpos == 17) ++nrows;
                        if(xpos == 5 || xpos == 13 || xpos == 21) ++ncols;
                    }
                    msw = (ncols > 2) ? ((master_size*2)/ncols) : master_size;
                    ssw = (sw - msw)/(ncols-1); ht = sh/nrows;
                    xpos = msw+(ssw*(ncols-2)); ypos = y+((nrows-1)*ht);
                    for(c=head;c->next;c=c->next);
                    for(d=c;d;d=d->prev) {
                        --x;
                        if(n == nrows) {
                            xpos -= (xpos == msw) ? msw : ssw;
                            ypos = y+((nrows-1)*ht);
                            n = 0;
                        }
                        if(x == 0) {
                            ht = (ypos-y+ht);
                            ypos = y;
                        }
                        if(x == 2 && xpos == msw && ypos != y) {
                            ht -= growth;
                            ypos += growth;
                        }
                        if(x == 1) {
                            ht += growth;
                            ypos -= growth;
                        }
                        wdt = (xpos > 0) ? ssw : msw;
                        XMoveResizeWindow(dis,d->win,scrx+xpos,scry+ypos,wdt-bdw,ht-bdw);
                        ht = sh/nrows;
                        ypos -= ht; ++n;
                    }
                }
                break;
            case 4: // Stacking
                    for(c=head;c;c=c->next)
                        XMoveResizeWindow(dis,c->win,scrx+c->x,scry+c->y,c->width,c->height);
                break;
            }
            default:
                break;
        }
    }
}

void update_current() {
    client *c, *d; unsigned int border, tmp = current_desktop, i;

    save_desktop(current_desktop);
    for(i=0;i<num_screens;++i) {
        if(view[i].cd != current_desktop) {
            select_desktop(view[i].cd);
            if(head != NULL) {
                XSetInputFocus(dis,root,RevertToParent,CurrentTime);
                XSetWindowBorder(dis,current->win,theme[1].wincolor);
                if(clicktofocus == 0)
                    XGrabButton(dis, AnyButton, AnyModifier, current->win, True, ButtonPressMask|ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None);
            }
        }
    }
    select_desktop(tmp);

    if(head == NULL && transient == NULL) return;

    if(head != NULL) {
        border = ((head->next == NULL && mode != 4) || (mode == 1)) ? 0 : bdw;
        for(c=head;c->next;c=c->next);
        for(d=c;d;d=d->prev) {
            XSetWindowBorderWidth(dis,d->win,border);
            if(d != current) {
                if(d->order < current->order) ++d->order;
                if(ufalpha < 100) XChangeProperty(dis, d->win, alphaatom, XA_CARDINAL, 32, PropModeReplace, (unsigned char *) &opacity, 1l);
                XSetWindowBorder(dis,d->win,theme[1].wincolor);
                if(clicktofocus == 0)
                    XGrabButton(dis, AnyButton, AnyModifier, d->win, True, ButtonPressMask|ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None);
            }
            else {
                // "Enable" current window
                if(ufalpha < 100) XDeleteProperty(dis, d->win, alphaatom);
                if(d == focus) {
                    XSetWindowBorder(dis,d->win,theme[0].wincolor);
                    XSetInputFocus(dis,d->win,RevertToParent,CurrentTime);
                    if(clicktofocus == 0)
                        XUngrabButton(dis, AnyButton, AnyModifier, d->win);
                } else {
                    XSetWindowBorder(dis,d->win,theme[1].wincolor);
                    if(clicktofocus == 0)
                        XGrabButton(dis, AnyButton, AnyModifier, d->win, True, ButtonPressMask|ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None);
                }
                XRaiseWindow(dis,d->win);
            }
        }
        current->order = 0;
    }
    if(transient != NULL) {
        for(c=transient;c->next;c=c->next);
        for(d=c;d;d=d->prev) {
            XMoveResizeWindow(dis,d->win,d->x,d->y,d->width,d->height);
            XSetWindowBorderWidth(dis,d->win,bdw);
            XRaiseWindow(dis,d->win);
            if(d == focus) {
                XSetWindowBorder(dis,d->win,theme[0].wincolor);
                if(ufalpha < 100) XDeleteProperty(dis, d->win, alphaatom);
                XSetInputFocus(dis,d->win,RevertToParent,CurrentTime);
            } else {
                XSetWindowBorder(dis,d->win,theme[1].wincolor);
                if(ufalpha < 100) XChangeProperty(dis, d->win, alphaatom, XA_CARDINAL, 32, PropModeReplace, (unsigned char *) &opacity, 1l);
            }
        }
    }
    if(STATUS_BAR == 0 && show_bar == 0) status_text(getwindowname(focus->win));
    warp_pointer();
    XSync(dis, False);
}

void switch_mode(const Arg arg) {
    if(mode == arg.i) return;

    client *c;
    growth = 0;
    if(mode == 1 && head != NULL) {
        XUnmapWindow(dis, current->win);
        for(c=head;c;c=c->next)
            XMapWindow(dis, c->win);
    }

    mode = arg.i;
    master_size = (mode == 2) ? (sh*msize)/100 : (sw*msize)/100;
    if(mode == 1 && head != NULL)
        for(c=head;c;c=c->next)
            XUnmapWindow(dis, c->win);

    save_desktop(current_desktop);
    tile();
    update_current();
    if(STATUS_BAR == 0) status_text("");
}

void resize_master(const Arg arg) {
    if(mode == 4 && current != NULL) {
        current->width = (current->width + arg.i > 50) ? current->width + arg.i: 50;
        XMoveResizeWindow(dis,current->win,desktops[current_desktop].x+current->x,
          current->y,current->width,current->height);
    } else if(mode == 1 || numwins < 2) return;
    else {
        if(arg.i > 0) {
            if((mode != 2 && sw-master_size > 70) || (mode == 2 && sh-master_size > 70))
                master_size += arg.i;
        } else if(master_size > 70) master_size += arg.i;
        tile();
    }
}

void resize_stack(const Arg arg) {
    if(mode == 4 && current != NULL) {
        current->height = (current->height + arg.i > 50) ? current->height + arg.i: 50;
        XMoveResizeWindow(dis,current->win,desktops[current_desktop].x+current->x,
          current->y,current->width,current->height);
    } else if(mode == 3) {
        if(arg.i > 0 && ((sh/2+growth) < (sh-100))) growth += arg.i;
        else if(arg.i < 0 && ((sh/2+growth) > 80)) growth += arg.i;
        tile();
    } else if(numwins > 2) {
        int n = numwins-1;
        if(arg.i >0) {
            if((mode != 2 && sh-(growth+sh/n) > (n-1)*70) || (mode == 2 && sw-(growth+sw/n) > (n-1)*70))
                growth += arg.i;
        } else {
            if((mode != 2 && (sh/n+growth) > 70) || (mode == 2 && (sw/n+growth) > 70))
                growth += arg.i;
        }
        tile();
    }
}

/* ********************** Keyboard Management ********************** */
void grabkeys() {
    unsigned int i;
    int j;
    XModifierKeymap *modmap;
    KeyCode code;

    XUngrabKey(dis, AnyKey, AnyModifier, root);
    XUngrabButton(dis, AnyButton, AnyModifier, root);
    read_keys_file();
    // numlock workaround
    numlockmask = 0;
    modmap = XGetModifierMapping(dis);
    for (i = 0; i < 8; ++i) {
        for (j = 0; j < modmap->max_keypermod; ++j) {
            if(modmap->modifiermap[i * modmap->max_keypermod + j] == XKeysymToKeycode(dis, XK_Num_Lock))
                numlockmask = (1 << i);
        }
    }
    XFreeModifiermap(modmap);

    // For each shortcut
    for(i=0;i<keycount;++i) {
        code = XKeysymToKeycode(dis,XStringToKeysym(keys[i].keysym));
        XGrabKey(dis, code, keys[i].mod, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, code, keys[i].mod | LockMask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, code, keys[i].mod | numlockmask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, code, keys[i].mod | numlockmask | LockMask, root, True, GrabModeAsync, GrabModeAsync);
    }
    for(i=1;i<4;i+=2) {
        XGrabButton(dis, i, resizemovekey, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(dis, i, resizemovekey | LockMask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(dis, i, resizemovekey | numlockmask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
        XGrabButton(dis, i, resizemovekey | numlockmask | LockMask, root, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    }
}

void keypress(XEvent *e) {
    unsigned int i;
    KeySym keysym;
    XKeyEvent *ev = &e->xkey;

    keysym = XkbKeycodeToKeysym(dis, (KeyCode)ev->keycode, 0, 0);
    for(i = 0; i < keycount; ++i) {
        if(keysym == XStringToKeysym(keys[i].keysym) && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)) {
            if(keys[i].myfunction)
                keys[i].myfunction(keys[i].arg);
        }
    }
}

void warp_pointer() {
    // Move cursor to the center of the current window
    if(followmouse != 0) return;
    if(dowarp < 1 && current != NULL) {
        XGetWindowAttributes(dis, current->win, &attr);
        XWarpPointer(dis, None, current->win, 0, 0, 0, 0, attr.width/2, attr.height/2);
        return;
    }
}

/* ********************** Signal Management ************************** */
int check_dock(Window w) {
    unsigned long count, j, extra;
    Atom realType;
    int realFormat, ret = 1;
    unsigned char *temp;
    Atom *type;

    if(XGetWindowProperty(dis, w, typeatom, 0, 32,
       False, XA_ATOM, &realType, &realFormat, &count, &extra,
        &temp) == Success) {
        if(count > 0) {
            type = (unsigned long*)temp;
            for(j=0; j<count; j++)
                if(type[j] == dockatom) ret = 0;
        }
        XFree(temp);
    }
    return ret;
}

void kill_client() {
    if(head == NULL && transient == NULL) return;
    unsigned int tr = 0; client *c; Window w = focus->win;
    kill_client_now(w);
    if(w) return;
    if(transient != NULL) {
        for(c=transient;c;c=c->next)
            if(c == focus) tr = 1;
    }
    remove_client(focus, 0, tr);
    update_current();
    if(STATUS_BAR == 0) update_bar();
}

void kill_client_now(Window w) {
    int n, i;
    XEvent ke;

    if (XGetWMProtocols(dis, w, &protocols, &n) != 0) {
        for(i=n;i>=0;--i) {
            if (protocols[i] == wm_delete_window) {
                ke.type = ClientMessage;
                ke.xclient.window = w;
                ke.xclient.message_type = protos;
                ke.xclient.format = 32;
                ke.xclient.data.l[0] = wm_delete_window;
                ke.xclient.data.l[1] = CurrentTime;
                XSendEvent(dis, w, False, NoEventMask, &ke);
            }
        }
    } else XKillClient(dis, w);
    XFree(protocols);
}

void quit() {
    unsigned int i, j;

    for(i=0;i<DESKTOPS;++i) {
        if(desktops[i].head != NULL) select_desktop(i);
        else continue;
        for(j=0;j<numwins;++j)
            kill_client();
    }
    XClearWindow(dis, root);
    XUngrabKey(dis, AnyKey, AnyModifier, root);
    for(i=0;i<10;++i)
        XFreeGC(dis, theme[i].gc);
    XFreePixmap(dis, area_sb);
    XSync(dis, False);
    XSetInputFocus(dis, root, RevertToPointerRoot, CurrentTime);
    logger("\033[0;34mYou Quit : Bye!");
    if(shutting_down > 0) return;
    else bool_quit = 1;
}

unsigned long getcolor(const char* color) {
    XColor c;
    Colormap map = DefaultColormap(dis,screen);

    if(XAllocNamedColor(dis,map,color,&c,&c)) return c.pixel;
    else {
        logger("\033[0;31mError parsing color!");
        return 1;
    }
    
}

void terminate(const Arg arg) {
    unsigned int i, j=0;
    char *search, *msg;
    Arg a;
    
    shutting_down = 1;
    quit();
    if(arg.i == 1) {
        search = "shutdowncmd";
        msg = "SHUTTING DOWN";
    } else {
        search = "rebootcmd";
        msg = "REBOOTING";
    }
    for(i=0;i<cmdcount;++i) {
        if(strcmp(search, cmds[i].name) == 0) {
            while(strncmp(cmds[i].list[j], "NULL", 4) != 0) {
                a.com[j] = cmds[i].list[j];
                ++j;
            }
            a.com[j] = NULL;
            logger(msg);
            bool_quit = 1;
            execvp((char*)a.com[0],(char**)a.com);
        }
    }
}

void logger(const char* e) {
    fprintf(stderr,"\n\033[0;34m:: snapwm : %s \033[0;m\n", e);
    fflush(stderr);
}

void plugnplay(XEvent *e) {
    client *c; unsigned int tmp = current_desktop;
    XRRUpdateConfiguration(e);
    if(font.fontset) XFreeFontSet(dis, font.fontset);
    memset(font_list, '\0', 256);
    XUngrabKey(dis, AnyKey, AnyModifier, root);
    for(i=0;i<10;++i)
        XFreeGC(dis, theme[i].gc);
    XFreePixmap(dis, area_sb);
    for(i=0;i<num_screens;++i) {
        select_desktop(view[i].cd);
        for(c=head;c;c=c->next) XUnmapWindow(dis, c->win);
        for(c=transient;c;c=c->next) XUnmapWindow(dis, c->win);
    }
    select_desktop(tmp);
    XSetInputFocus(dis, root, RevertToPointerRoot, CurrentTime);
    XSync(dis, False);
    XCloseDisplay(dis);
    dis = XOpenDisplay(NULL);
    screen = DefaultScreen(dis);
    root = RootWindow(dis,screen);
    read_rcfile();
    init_desks();
    if(STATUS_BAR == 0) {
        setup_status_bar();
        status_bar();
        if(show_bar > 0) unmapbar();
    }
    grabkeys();
    init_start();
    for(i=0;i<num_screens;++i) {
        select_desktop(view[i].cd);
        for(c=head;c;c=c->next) XMapWindow(dis, c->win);
        tile();
        for(c=transient;c;c=c->next) XMapWindow(dis, c->win);
    }
    Arg a = {.i = tmp};
    change_desktop(a);
    update_current();
}

void init_start() {
    alphaatom = XInternAtom(dis, "_NET_WM_WINDOW_OPACITY", False);
    wm_delete_window = XInternAtom(dis, "WM_DELETE_WINDOW", False);
    protos = XInternAtom(dis, "WM_PROTOCOLS", False);
    typeatom = XInternAtom(dis, "_NET_WM_WINDOW_TYPE", False);
    dockatom = XInternAtom(dis, "_NET_WM_WINDOW_TYPE_DOCK", False);
    // To catch maprequest and destroynotify (if other wm running)
    XSetWindowAttributes at;
    at.event_mask = SubstructureNotifyMask|SubstructureRedirectMask|PropertyChangeMask;
    XChangeWindowAttributes(dis,root,CWEventMask,&at);
    XRRSelectInput(dis,root,RRScreenChangeNotifyMask);
    int err;
    XRRQueryExtension(dis,&randr_ev,&err);
}

void init_desks() {
    int last_width=0, i, j, have_Xin = 0;

    XineramaScreenInfo *info = NULL;
    if(!(info = XineramaQueryScreens(dis, &num_screens))) {
        logger("XINERAMA Fail");
        num_screens = 1;
        have_Xin = 1;
    }
    //fprintf(stderr, "Number of screens is %d -- HAVE_XIN = %d\n", num_screens, have_Xin);

    if(barmon != barmonchange && barmonchange >= 0 && barmonchange < num_screens)
        barmon = barmonchange;
    for (i = 0; i < num_screens; ++i) {
        for(j=i;j<DESKTOPS;j+=num_screens) {
            if(i == barmon && STATUS_BAR == 0) {
                desktops[j].h = ((have_Xin == 0) ? info[i].height:XDisplayHeight(dis, screen)) - (sb_height+4+bdw);
                desktops[j].showbar = 0;
            } else {
                desktops[j].h = ((have_Xin == 0) ? info[i].height:XDisplayHeight(dis, screen)) - bdw;
                desktops[j].showbar = 1;
            }
            if(!(desktops[j].mode)) desktops[j].mode = mode;
            if(!(desktops[j].nmaster)) desktops[j].nmaster = nmaster;
            //fprintf(stderr, "**screen is %d - desktop is %d **\n", i, j);
            desktops[j].x = (have_Xin == 0) ? info[i].x_org + last_width:0;
            desktops[j].y = (have_Xin == 0) ? info[i].y_org:0;
            desktops[j].w = (have_Xin == 0) ? info[i].width - bdw:XDisplayWidth(dis, screen);
            //fprintf(stderr, " x=%d - y=%d - w=%d - h=%d \n", desktops[j].x, desktops[j].y, desktops[j].w, desktops[j].h);
            desktops[j].master_size = (desktops[j].mode == 2) ? (desktops[j].h*msize)/100 : (desktops[j].w*msize)/100;
            if(!(desktops[j].growth)) desktops[j].growth = 0;
            if(!(desktops[j].numwins)) desktops[j].numwins = 0;
            if(!(desktops[j].head)) desktops[j].head = NULL;
            if(!(desktops[j].current)) desktops[j].current = NULL;
            if(!(desktops[j].transient)) desktops[j].transient = NULL;
            if(!(desktops[j].focus)) desktops[j].focus = NULL;
            desktops[j].screen = i;
        }
        last_width += desktops[j].w;
        view[i].cd = i;
    }
    XFree(info);
}

void setup() {
    // Install a signal
    sigchld(0);

    // Screen and root window
    screen = DefaultScreen(dis);
    root = RootWindow(dis,screen);

    // Initialize variables
    DESKTOPS = 4;
    topbar = followmouse = top_stack = mode = cstack = default_desk = 0;
    LA_WINDOWNAME = wnamebg = dowarp = doresize = nmaster = has_bar = 0;
    auto_mode = auto_num = shutting_down = 0;
    msize = 55;
    ufalpha = 75; baralpha = 90;
    bdw = 2;
    showopen = clicktofocus = attachaside = 1;
    resizemovekey = Mod1Mask;
    windownamelength = 35;
    show_bar = STATUS_BAR = barmon = barmonchange = lessbar = 0;

    char *loc;
    loc = setlocale(LC_ALL, "");
    if (!loc || !strcmp(loc, "C") || !strcmp(loc, "POSIX") || !XSupportsLocale())
        logger("LOCALE FAILED");
    // Read in RC_FILE
    sprintf(RC_FILE, "%s/.config/snapwm/rc.conf", getenv("HOME"));
    sprintf(KEY_FILE, "%s/.config/snapwm/key.conf", getenv("HOME"));
    sprintf(APPS_FILE, "%s/.config/snapwm/apps.conf", getenv("HOME"));
    set_defaults();
    read_rcfile();

    // Set up all desktops
    init_desks();

    if(STATUS_BAR == 0) {
        setup_status_bar();
        status_bar();
        update_output(1);
        if(show_bar > 0) unmapbar();
    }
    read_apps_file();

    // Shortcuts
    grabkeys();

    // For exiting
    bool_quit = 0;

    // Select default desktop
    select_desktop(0);
    if(default_desk > 0) {
        Arg a = {.i = default_desk};
        change_desktop(a);
    }
    init_start();
    update_current();
    setbaralpha();

    logger("\033[0;32mWe're up and running!");
}

void sigchld(int unused) {
    if(signal(SIGCHLD, sigchld) == SIG_ERR) {
        logger("\033[0;31mCan't install SIGCHLD handler");
        exit(1);
        }
    while(0 < waitpid(-1, NULL, WNOHANG));
}

void spawn(const Arg arg) {
    if(fork() == 0) {
        if(fork() == 0) {
            if(dis) close(ConnectionNumber(dis));
            setsid();
            execvp((char*)arg.com[0],(char**)arg.com);
        }
        exit(0);
    }
}


void start() {
    XEvent ev;

    while(!bool_quit && !XNextEvent(dis,&ev)) {
        if(ev.type == randr_ev) plugnplay(&ev);
        else if(events[ev.type])
            events[ev.type](&ev);
    }
}

int main() {
    // Open display
    if(!(dis = XOpenDisplay(NULL))) {
        logger("\033[0;31mCannot open display!");
        exit(1);
    }

    XSetErrorHandler(xerror);

    // Setup env
    setup();

    // Start wm
    start();

    // Close display
    XCloseDisplay(dis);

    exit(0);
}
