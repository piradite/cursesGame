#define _POSIX_C_SOURCE 199309L
#include <curses.h> // This may not be as portable
#include <errno.h> // This may not be as portable
#include <math.h>
#include <pthread.h> // This may not be as portable
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h> // This may not be as portable

#define CRASH(errnum) {fprintf(stderr, "FATAL ERROR (line %d). Code: %s\n", __LINE__, strerror(errnum));endwin();printf("\033[?1003l\n");exit(errnum);}
#define WARN(msg) {fprintf(stderr, "Warning: %s Might crash soon... (line: %d)\n", msg, __LINE__);}
#define mLINES 17 //	Max Screen size
#define mCOLS 39 // Max screen size
#define MAX_FILE_NAME_SIZE 16 // File name size
#define TARGET_TICK_RATE 20 
#define TARGET_FRAME_RATE 60
#define TARGET_INPUT_RATE 90
#define HEADER_SIZE 50 //Size of header in each read file
#define MAX_NAME_SIZE 32// Never use this size. It is just a temporary buffer

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#define NS_WAIT 1000000000
#define US_WAIT 1000000
#define MS_WAIT 1000
#define S_WAIT 1

// #! Global
const float g_tickPeriod=1000.0/TARGET_TICK_RATE;
const float g_framePeriod=1000.0/TARGET_FRAME_RATE;
const float g_inputPeriod=1000.0/TARGET_INPUT_RATE;

// #! Structs
struct _Vector {
	int *coord; // coorindate
	int degree; // number of coordinates
};

struct shape_vertex {
	struct _Vector **vertex; // each coordinate of each point
	int pointc; // number of points
};

struct entity {
	int ex, ey; // X and Y position
	int r, t; // Range and Angle (theta) for direction facing and reach
} entity;

struct map {
	char *mapName;
	char mapId[MAX_FILE_NAME_SIZE];
	short *mapArr;
	int cols, lines;
	int size;
} map;

struct player {
	struct entity *self;
	char saveId[MAX_FILE_NAME_SIZE];
	char Name[20];

	// Onscreen location of the character
	int screenx, screeny;

	// Spawn/respawning
	int resx, resy;
	char *last_open_mapId;
} player;

struct data {
	char *tileset;
} data;

struct base {
	struct data *dat;
} base;

struct arg {
	struct base *self;
	struct player* p;
	struct map *currentMap;
	WINDOW *window_array[5];

	short isRunning;
	short iSMenu;
	short autopause; // Pause during menu option
	unsigned long long tick, frame;
	int cKey; // Keyboard input
	int mX, mY; // Cursor coordinates

	enum State {world, menu, inventory} gameState;

	// Frame and tick time
	clock_t preFrame, postFrame;
	clock_t preTick, postTick;
	
	int cornerCoords[2 * 2 * 2];
	int wOffset, hOffset;
	pthread_mutex_t player_mutex;
} args;

// #!Functions
void generic_delay(const unsigned long int ms, const unsigned long int unit);
int generic_drawLine(int x0, int y0, int x1, int y1, struct shape_vertex *shape);
int generic_drawLine_polar(const int xi, const int yi, const int theta, const int range, struct shape_vertex *shape);
void generic_portableSleep(const int ms);

struct arg *main_init(void);
void main_loop(struct arg *args);
// TODO: add return code in the args for debug purposes
void *main_loopCalculation(void *args);
void *main_loopDisplay(void *args);
void *main_loopInput(void *args);

void util_displayShape(WINDOW *window, struct shape_vertex *shape, char material);
void util_loadMap(char *path, char *mapId, struct map *currentMap);
void *util_resizePointer(void *pointer, size_t new_size);
int util_cleanShape(struct shape_vertex *shape);

int world_checkCollision(int wx, int wy, struct map *currentMap);
void world_defineCorners(int px, int py, int *output);
void world_display(struct arg *args);
enum State world_loop(struct arg *args);

// This is a bad function. Don't use this...
void 
generic_delay(const unsigned long int time, const unsigned long int unit) {
	clock_t end=clock()+time*(CLOCKS_PER_SEC / unit);
	while (clock()<end) {}
	return;
}

int
generic_drawLine(int x0, int y0, int x1, int y1, struct shape_vertex *shape) {
	int dx=x1-x0;
	int dy=y1-y0;
	int dir=1;
	if (dx<0) dx*=-1;
	if (dy<0) dy*=-1;
	if (dx>dy) {
		shape->vertex=malloc(sizeof(struct _Vector *)*(dx+1));
		if (!shape->vertex) CRASH(ENOMEM);
		fprintf(stdout, "dx num: %d, (%d)\n", dx, dx+1);
		shape->pointc=dx+1;
		if (x0>x1) {
			int tmp=x0;x0=x1;x1=tmp;tmp=y0;y0=y1;y1=tmp;
			}
		int dx=x1-x0;int dy=y1-y0;
		if (dy<1) dir=-1; 
		dy*=dir;
		if (dx!=0) {
			int yc=y0;int p=2*dy-dx;
			for (int i=0;i<dx+1;i++) {
				shape->vertex[i]=malloc(sizeof(struct _Vector));
				if (!shape->vertex[i]) CRASH(ENOMEM);
				shape->vertex[i]->degree=2;
				shape->vertex[i]->coord=malloc(sizeof(int)*shape->vertex[i]->degree);
				if (!shape->vertex[i]->coord) CRASH(ENOMEM);
				shape->vertex[i]->coord[0]=x0+i;
				shape->vertex[i]->coord[1]=yc;
				if (p>=0){
					yc+=dir;p=p-2*dx;
				} p=p+2*dy;
			}
			return 1;
		}
	} else if (dy>dx){
		shape->vertex=malloc(sizeof(struct _Vector *)*(dy+1));
		if (!shape->vertex) CRASH(ENOMEM);
		fprintf(stdout, "dy num: %d, (%d)\n", dy, dy+1);
		shape->pointc=dy+1;
		if (y0>y1) {
		int tmp=x0;x0=x1;x1=tmp;tmp=y0;y0=y1;y1=tmp;
		}
		int dx=x1-x0;int dy=y1-y0;
		if (dx<1) dir=-1; 
		dx*=dir;
		if (dy!=0) {
			int xc=x0;int p=2*dx-dy;
			for (int i=0;i<dy+1;i++) {
				shape->vertex[i]=malloc(sizeof(struct _Vector));
				if (!shape->vertex[i]) CRASH(ENOMEM);
				shape->vertex[i]->degree=2;
				shape->vertex[i]->coord=malloc(sizeof(int)*shape->vertex[i]->degree);
				if (!shape->vertex[i]->coord) CRASH(ENOMEM);
				shape->vertex[i]->coord[0]=xc;
				shape->vertex[i]->coord[1]=y0+i;
				if (p>=0){
					xc+=dir;p=p-2*dy;
				} p=p+2*dx;
			}
			return 1;
		}
	} else {
		shape->vertex=malloc(sizeof(struct _Vector *)*dy);
		if (!shape->vertex)	CRASH(ENOMEM);
		fprintf(stdout, "dy num: %d, dx num: %d (should be equal)\n", dy, dx);
		shape->pointc=dy;
		if (y0>y1) {
		int tmp=x0;x0=x1;x1=tmp;tmp=y0;y0=y1;y1=tmp;
		}
		int yc=y0;int p=2*dy-dx;
		for (int i=0;i<dx;i++) {
			shape->vertex[i]=malloc(sizeof(struct _Vector));
			if (!shape->vertex[i]) CRASH(ENOMEM);
			shape->vertex[i]->degree=2;
			shape->vertex[i]->coord=malloc(sizeof(int)*shape->vertex[i]->degree);
			if (!shape->vertex[i]->coord) CRASH(ENOMEM);
			shape->vertex[i]->coord[0]=x0+i;
			shape->vertex[i]->coord[1]=yc;
			if (p>=0){
				yc+=dir;p=p-2*dx;
			} p=p+2*dy;
		}
	}
	return 0;
}

/*This function returns the return code from the inner function. NOT A SHAPE ARRAY*/
int
generic_drawLine_polar(const int xi, const int yi, const int theta, const int range, struct shape_vertex *shape) {
	int endpt[2]={round(xi-range * (cos(theta))), round(yi -range * (sin(theta)))};
	return generic_drawLine(xi, yi, endpt[0], endpt[1], shape);
}

void 
generic_portableSleep(const int ms) {
	#ifdef WIN32
		Sleep(ms);
	#elif _POSIX_C_SOURCE >= 199309L
		struct timespec ts;
		ts.tv_sec = ms / 1000;
		ts.tv_nsec = (ms % 1000) * 1000000;
		nanosleep(&ts, NULL);
	#endif
		//usleep(ms * 1000);
}

struct arg *
main_init(void) {
	initscr();noecho();cbreak();clear();curs_set(0);keypad(stdscr, TRUE);
	struct arg *opt=malloc(sizeof(struct arg));
	if (!opt) CRASH(ENOMEM);
	opt->p=malloc(sizeof(player));
	if (!opt->p) CRASH(ENOMEM);
	opt->p->self=malloc(sizeof(entity));
	if (!opt->p->self) CRASH(ENOMEM);
	opt->self=malloc(sizeof(base));
	if (!opt->self) CRASH(ENOMEM);
	opt->self->dat=malloc(sizeof(data));
	if (!opt->self->dat) CRASH(ENOMEM);
	// set 5 tilesets for now... move into dat file later
	opt->self->dat->tileset=malloc(sizeof(char)*6);
	if (!opt->self->dat->tileset) CRASH(ENOMEM);
	if (!strncpy(opt->self->dat->tileset, ".#><", 7)) CRASH(ENOMEM);

	opt->currentMap=malloc(sizeof(map));
	if (!opt->currentMap) CRASH(ENOMEM);
	opt->isRunning=1;	

	opt->window_array[0]=newwin(mLINES, mCOLS, 0, 0); // Root Window (always on)
	opt->window_array[1]=subwin(opt->window_array[0], 0, 0, 0, 0); // main UI (always on)
	opt->window_array[2]=subwin(opt->window_array[0], 0, 0, 0, 0); // World Display
	opt->window_array[3]=subwin(opt->window_array[2], 0, 0, 0, 0); // World UI (world display)

	opt->gameState=world;
	opt->p->self->ey=5;
	opt->p->self->ex=5;

	timeout(0);
	pthread_mutex_init(&opt->player_mutex, NULL);
	// Move loading map to here
	// TODO: load data/map/player here later
	
	// Initialize mouse input
	keypad(stdscr, TRUE);
	mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION,NULL);
	printf("\033[?1003h\n"); // Mouse caputre escape sequence

	opt->tick=0;
	return opt;
}

// TODO: Deprecated. migrate n remove
void
main_loop(struct arg *args) {
	pthread_t th_display, th_input, th_calc;

	pthread_create(&th_calc, NULL, main_loopCalculation, (void *)args);
	pthread_create(&th_display, NULL, main_loopDisplay, (void *)args);
	pthread_create(&th_input, NULL, main_loopInput, (void *)args);
//	while (args->isRunning) {}

	pthread_join(th_calc, NULL);
	pthread_join(th_display, NULL);
	pthread_join(th_input, NULL);

}

void *
main_loopCalculation(void *args) {
	struct arg *cArgs=(struct arg *)args;
	clock_t preTick, postTick;
	while (cArgs->isRunning) {
		preTick=clock();
		cArgs->tick++;
		//timespec_get(cArgs->preTick, TIME_UTC);
		
		switch (cArgs->gameState) {
			case world:	
				world_loop(cArgs);
				break;
			case inventory:
				break;
			case menu:
				break;
			default:
				break;
			wrefresh(cArgs->window_array[2]);
		}
		postTick=clock();
		int wait=g_framePeriod-(double)(postTick-preTick)*1000.0/CLOCKS_PER_SEC;
		generic_portableSleep(wait);
	}	
	return NULL;
}

void *
main_loopDisplay(void *args) {
	struct arg *cArgs=(struct arg *)args;
	clock_t preFrame, postFrame;
	box(cArgs->window_array[0], 0, 0);
	while (cArgs->isRunning) {
		preFrame=clock();
		cArgs->frame++;
		cArgs->wOffset=cArgs->hOffset=1;
		if (mLINES%2) cArgs->wOffset=2;
		if (mCOLS%2) cArgs->hOffset=2;
		
		switch (cArgs->gameState) {
			case world:	
				wclear(cArgs->window_array[2]);
				world_display(cArgs);
				break;
			case inventory:
				break;
			case menu:
				break;
			default:
				break;
			wrefresh(cArgs->window_array[2]);
		}
		mvwprintw(cArgs->window_array[0],cArgs->mY, cArgs->mX, "X");
		wrefresh(cArgs->window_array[0]);
		wrefresh(cArgs->window_array[1]);

		postFrame=clock();
		int wait=g_framePeriod-(double)(postFrame-preFrame)*1000.0/CLOCKS_PER_SEC;
		generic_portableSleep(wait);
	}
	return NULL;
}

void *main_loopInput(void *args) {
    struct arg *cArgs = (struct arg *)args;
    clock_t preInput, postInput;
    while (cArgs->isRunning) {
        preInput = clock();
        int key = getch();
        if (key == KEY_MOUSE) {
            MEVENT mouse_event;
            if (getmouse(&mouse_event) == OK) {
                cArgs->mY = mouse_event.y;
                cArgs->mX = mouse_event.x;
            }
        } else {
            int dx = 0, dy = 0;
            switch (key) {
                case 'w': dy = -1; break;
                case 's': dy = 1; break;
                case 'a': dx = -1; break;
                case 'd': dx = 1; break;
                case 'q': cArgs->isRunning = 0; break;
            }
            if (dx != 0 || dy != 0) {
                pthread_mutex_lock(&cArgs->player_mutex);
                int new_ex = cArgs->p->self->ex + dx;
                int new_ey = cArgs->p->self->ey + dy;
                // Check collision
                if (!world_checkCollision(new_ex, new_ey, cArgs->currentMap)) {
                    cArgs->p->self->ex = new_ex;
                    cArgs->p->self->ey = new_ey;
                }
                pthread_mutex_unlock(&cArgs->player_mutex);
            }
        }
        postInput = clock();
        int wait = g_inputPeriod - (double)(postInput - preInput) * 1000.0 / CLOCKS_PER_SEC;
        if (wait > 0) {
            generic_portableSleep(wait);
        }
    }
    return NULL;
}

int
world_checkCollision(int wx, int wy, struct map *currentMap) {
	int tile=currentMap->mapArr[wy * currentMap->cols + wx];
		if (tile<2) return tile;
		switch (tile) {
			default:
				return 0;
				break;
		}
		return 0;
}

void
world_defineCorners(int px, int py, int * output) {
	/* (0,1)	(2,3)
	 * (4,5)	(6,7)
	 * */
	if (mLINES%2) {
		output[5]=py+(int)(mLINES/2)-1; // Y coord
	} else { output[5]=py+(int)(mLINES/2)-2; } // Y coord
	if (mCOLS%2) {
	output[2]=px+(int)(mCOLS/2)-1;  // X coord
	} else { output[2]=px+(int)(mCOLS/2)-2; } // X coord
										  //
	output[0]=px-(int)(mCOLS/2)+1;  // X coord
	output[1]=py-(int)(mLINES/2)+1; // Y coord
	output[3]=output[1];
	output[4]=output[0];
	output[6]=output[2];
	output[7]=output[5];
}

void
world_display(struct arg *args) {
    int edgeX, edgeY;
    pthread_mutex_lock(&args->player_mutex);
    edgeX = args->p->self->ex;
    edgeY = args->p->self->ey;
    pthread_mutex_unlock(&args->player_mutex);

	int midX=(int)mLINES/2, midY=(int)mCOLS/2;

	// Calculate whether to scroll, or move the player on screen. 
	if (args->p->self->ex-mCOLS/2+args->wOffset<=0) {
		edgeX=mCOLS/2-args->wOffset+1;
	} else if (args->p->self->ex+mCOLS/2-1>=args->currentMap->cols) edgeX=args->currentMap->cols-mCOLS/2;
	if (args->p->self->ey-mLINES/2+args->hOffset<=0) {
		edgeY=mLINES/2-args->hOffset+1;
	} else if (args->p->self->ey+mLINES/2-1>=args->currentMap->lines) edgeY=args->currentMap->lines-mLINES/2;

	// define edges to draw from
	world_defineCorners(edgeX, edgeY, args->cornerCoords);

	for (int iwx=args->cornerCoords[0], isx=1; /*iwx<args->cornerCoords[2],*/ isx < mCOLS-1;iwx++,isx++) {
		for (int iwy=args->cornerCoords[1], isy=1; /*iwy<args->cornerCoords[5],*/ isy < mLINES-1;iwy++, isy++) {
			mvwprintw(args->window_array[2], isy, isx, "%c", args->self->dat->tileset[args->currentMap->mapArr[iwy * args->currentMap->cols + iwx]]);

			/* Query the screen coordinates for when the map coordinates match up with player's map coordinates*/
			if (iwy==args->p->self->ey) midY=isy;
			if (iwx==args->p->self->ex) midX=isx;
		}
	}
	//mvwprintw(args->window_array[2], midY, midX, "@");
	struct shape_vertex *cursorLine=malloc(sizeof(struct shape_vertex));
	if (!generic_drawLine(midX, midY, args->mX, args->mY, cursorLine)) WARN("Some issue occured and shape was not drawn. ");
	for (int i=0;i<cursorLine->pointc;i++) {
	}
	util_displayShape(args->window_array[2], cursorLine,'0');
	//util_displayShape(args->window_array[2], generic_drawLine(midX, midY, args->mX, args->mY),'0');
	mvwaddch(args->window_array[2], midY, midX, '@');

	wrefresh(args->window_array[2]);
	wrefresh(args->window_array[1]);
}

enum State
world_loop(struct arg *args) { 
    (void)args;
    return world; 
}

void
util_displayShape(WINDOW *window, struct shape_vertex *shape, char material) {
	for (int i=0;i<shape->pointc;i++) {
		mvwprintw(window, shape->vertex[i]->coord[1], shape->vertex[i]->coord[0], "%c", material);
	}
}

void 
util_loadMap(char *path, char *mapId, struct map *currentMap) {
	FILE *fptr;
	char fullpath[256];

	// /path/to/data/dir/LEVEL/LEVEL1...LEVEL2... 
	if (snprintf(fullpath, 256, "%s/LEVEL/%s", path, mapId)<0) CRASH(ENOBUFS);
	if (access(fullpath, R_OK)!=0) CRASH(EACCES);
	if (!(fptr=fopen(fullpath, "rb"))) CRASH(ENOMEM);
	
	//char *raw=malloc(sizeof(char)*(HEADER_SIZE+1));
	//if (!raw) CRASH(ENOMEM);
	//if (!fread(raw, sizeof(char)*(HEADER_SIZE+1), 1, fptr)) CRASH(0);
	char *header=malloc(sizeof(char)*(HEADER_SIZE+1));
	if (!header) CRASH(ENOMEM);
	if (!fread(header, sizeof(char)*(HEADER_SIZE), 1, fptr)) CRASH(0);

	//strncpy(header, raw, sizeof(char)*(HEADER_SIZE+1));

	char tmp_nameBuffer[MAX_NAME_SIZE];
	if (!strncpy(currentMap->mapId, mapId, sizeof(char)*MAX_FILE_NAME_SIZE)) CRASH(ENOBUFS);
	if (strcmp(currentMap->mapId, strtok(header, "|"))!=0) fprintf(stderr, "Warning: map id doesn't seem correct...\n");
	if (!strncpy(tmp_nameBuffer, strtok(NULL, "|"), MAX_FILE_NAME_SIZE)) CRASH(ENOBUFS);
	currentMap->lines=atoi(strtok(NULL, "|"));
	currentMap->cols=atoi(strtok(NULL, "|"));
	currentMap->size=atoi(strtok(NULL, "|"));

	currentMap->mapName=malloc(sizeof(char)*strlen(tmp_nameBuffer));
	if (!currentMap->mapName) CRASH(ENOMEM);
	if (!strncpy(currentMap->mapName, tmp_nameBuffer, sizeof(char)*strlen(tmp_nameBuffer))) CRASH(ENOMEM);
	
	if (!(currentMap->mapArr=malloc(sizeof(short)*currentMap->lines*currentMap->cols))) CRASH(ENOMEM);
	for (int i=0;i<currentMap->lines;i++) {
		if (!fread(currentMap->mapArr+(i*50), sizeof(short)*currentMap->cols, 1, fptr)) CRASH(ENOMEM);
	}

	fclose(fptr);

	//free(raw);
	free(header);
	return;
}

void *
util_resizePointer(void *pointer, size_t new_size) {
	void *realloc_res=realloc(pointer, new_size);
	if (!realloc_res) {
		CRASH(ENOMEM)
	} return realloc_res;
}

int
main(int argc, char **argv) {
	fprintf(stdout, "args: %d, running: %s", argc, argv[0]);
	struct arg *args=malloc(sizeof(struct arg));
	args=main_init();

	util_loadMap("./data", "MAP000.TST", args->currentMap);

	refresh();

	main_loop(args);

	endwin();
return 0;
}

/**
 * Its a game engine. Made by me :)
 *
 *
 * **/