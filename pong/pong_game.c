/** \file pong_game.c
 *  A simple 2 player pong game which increases the ball speed anytime a paddle
 *  collides with it.
 */

#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <p2switches.h>
#include <shape.h>
#include <abCircle.h>
#include "buzzer.h"
#include "gameOver.h"

#define GREEN_LED BIT6

#include <msp430.h>
#include "p2switches.h"

short goal = 1; // checks if any player has made a goal
char p1Score = 0; // player1 score tracker
char p2Score = 0; // player2 score tracker
char player1Score = '0'; // player1 score tracker
char player2Score = '0'; // player2 score tracker
char playGame = 0; //checks to see if game is still in play
AbRect paddle = {abRectGetBounds, abRectCheck, {15,3}}; /**< 15x3 rectangle */
AbRect middleLine = {abRectGetBounds, abRectCheck, {61, 0}}; // horizontal line

AbRectOutline fieldOutline = {	/* playing field */
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2 - 2, screenHeight/2 - 6}
};


Layer fieldLayer = {		/* playing field as a layer */
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2 -3},/**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_WHITE,
  0
};

Layer layer4 = {		/**< Layer with a horizontal line */
  (AbShape *)&middleLine,
  {(screenWidth/2 ), (screenHeight/2 - 3)}, /**< Middle field divider */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_WHITE,
  &fieldLayer,
};		     

Layer layer3 = {		/**< Layer with a white paddle */
  (AbShape *)&paddle,
  {(screenWidth/2), (screenHeight/2)-70}, /** Top of Screen */
  {0,0}, {52,10},				    /* last & next pos */
  COLOR_WHITE,
  &layer4,
};


Layer layer2 = {		/**< Layer with a green circle */
  (AbShape *)&circle4,
  {screenWidth/2, screenHeight/2}, /**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_WHITE,
  &layer3,
};


Layer layer0 = {		/**< Layer with a white paddle */
  (AbShape *)&paddle,
  {(screenWidth/2), (screenHeight/2)+64}, /**< Bottom of Screen */
  {0,0}, {52,144},				    /* last & next pos */
  COLOR_WHITE,
  &layer2,
};

/** Moving Layer
 *  Linked list of layer references
 *  Velocity represents one iteration of change (direction & magnitude)
 */
typedef struct MovLayer_s {
  Layer *layer;
  Vec2 velocity;
  struct MovLayer_s *next;
} MovLayer;

/* initial value of {0,0} will be overwritten */

// Ball Layer ml1
MovLayer ml1 = { &layer2, {5,5}, 0 };

// Bottom paddle Layer
MovLayer ml2 = { &layer0, {5,5}, 0 };

// Upper paddle Layer
MovLayer ml3 = { &layer3, {5,5}, 0 };



//Function movLayerDraw() & m1Advance() implemented from Lab3 "shape-motion_demo" shapemotion.c

int movLayerDraw(MovLayer *movLayers, Layer *layers)
{
  int row, col;
  MovLayer *movLayer;

  and_sr(~8);			/**< disable interrupts (GIE off) */
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Layer *l = movLayer->layer;
    l->posLast = l->pos;
    l->pos = l->posNext;
  }
  or_sr(8);			/**< disable interrupts (GIE on) */


  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Region bounds;
    layerGetBounds(movLayer->layer, &bounds);
    lcd_setArea(bounds.topLeft.axes[0], bounds.topLeft.axes[1], 
		bounds.botRight.axes[0], bounds.botRight.axes[1]);
    for (row = bounds.topLeft.axes[1]; row <= bounds.botRight.axes[1]; row++) {
      for (col = bounds.topLeft.axes[0]; col <= bounds.botRight.axes[0]; col++) {
	Vec2 pixelPos = {col, row};
	u_int color = bgColor;
	Layer *probeLayer;
	for (probeLayer = layers; probeLayer; 
	     probeLayer = probeLayer->next) { /* probe all layers, in order */
	  if (abShapeCheck(probeLayer->abShape, &probeLayer->pos, &pixelPos)) {
	    color = probeLayer->color;
	    break; 
	  } /* if probe check */
	} // for checking all layers at col, row
	lcd_writeColor(color); 
      } // for col
    } // for row
  } // for moving layer being updated
}	  


Region fence = {{0,LONG_EDGE_PIXELS}, {SHORT_EDGE_PIXELS, LONG_EDGE_PIXELS}}; /**< Create a fence region */


/** Modified function by Eric Freudenthal Advances a moving shape within a rectangular fence,
 * incorporates logic statements to check if ball layer has collided with any of the paddles where it will then increase velocity,
 * If it collides with the external fence it will tally a score to the corresponding player,
 * player who gets scored on will be highlighted in red
 *
 *  \param ml The moving ball to be advanced
 *  \param ml1 The moving paddle to be advanced
 *  \param ml2 The moving paddle to be advanced
 *  \param fence The region which will serve as a boundary for ml
 */
void mlAdvance(MovLayer *ml, MovLayer *ml1, MovLayer *ml2, Region *fence) {
    Vec2 newPos;

    u_char axis;
    Region shapeBoundary;

    drawString5x7(3, 152, "Player1:", COLOR_GOLD, COLOR_VIOLET);
    drawString5x7(72, 152, "Player2:", COLOR_BLACK, COLOR_VIOLET);

    for (; ml; ml = ml->next) {
        vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
        abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
        for (axis = 0; axis < 2; axis++) {
            if ((shapeBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis]) ||
                (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis])) {
                int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
                buzzer_set_period(0);


                newPos.axes[axis] += (2 * velocity);
            }    /**< if outside of fence */

            // Check if ball has collided with top paddle
            if ((ml->layer->posNext.axes[1] >= 134) &&
                (ml->layer->posNext.axes[0] <= ml1->layer->posNext.axes[0] + 18 &&
                 ml->layer->posNext.axes[0] >= ml1->layer->posNext.axes[0] - 18)) {
                int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
                ml1->layer->color = COLOR_GOLD;
                ml2->layer->color = COLOR_WHITE;
                ml->layer->color = COLOR_GOLD;
                ml->velocity.axes[0] += 1;
                newPos.axes[axis] += (2 * velocity);

                buzzer_set_period(600);
                int redrawScreen = 1;
            }

                // Check if ball has collided with bottom paddle
            else if ((ml->layer->posNext.axes[1] <= 21) &&
                     (ml->layer->posNext.axes[0] <= ml2->layer->posNext.axes[0] + 18 &&
                      ml->layer->posNext.axes[0] >= ml2->layer->posNext.axes[0] - 18)) {
                int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
                ml2->layer->color = COLOR_BLACK;
                ml1->layer->color = COLOR_WHITE;
                ml->layer->color = COLOR_BLACK;
                ml->velocity.axes[0] += 1;
                newPos.axes[axis] += (2 * velocity);

                buzzer_set_period(500);
                int redrawScreen = 1;
            }

                // Check if ball has collided with upper fence
            else if ((ml->layer->posNext.axes[1] == 20)) {
                ml2->layer->color = COLOR_RED;
                player1Score++;
                p1Score++;
                drawString5x7(3, 152, "Player1:", COLOR_GOLD, COLOR_VIOLET);
                drawChar5x7(52, 152, player1Score, COLOR_GOLD, COLOR_VIOLET);

                newPos.axes[0] = screenWidth / 2;
                newPos.axes[1] = (screenHeight / 2);
                goal = 1;
                ml->velocity.axes[0] = 5;
                ml->layer->posNext = newPos;
                int redrawScreen = 1;
            }

                // Check if ball has collided with lower fence
            else if ((ml->layer->posNext.axes[1] == 135)) {
                ml1->layer->color = COLOR_RED;
                player2Score++;
                p2Score++;

                drawString5x7(72, 152, "Player2:", COLOR_BLACK, COLOR_VIOLET);
                drawChar5x7(120, 152, player2Score, COLOR_BLACK, COLOR_VIOLET);

                newPos.axes[0] = screenWidth / 2;
                newPos.axes[1] = (screenHeight / 2);
                goal = 1;
                ml->velocity.axes[0] = 5;
                ml->layer->posNext = newPos;
                int redrawScreen = 1;

            }

            int redrawScreen = 1;

            //If no player goal keep on updating the ball's position
            if (goal != 1) {
                ml->layer->posNext = newPos;
            }
        } /**< for axis */
    } /**< for ml */
}


u_int bgColor = COLOR_VIOLET;     // The background color
int redrawScreen = 1;           // Boolean for whether screen needs to be redrawn

Region fieldFence;        // fence around playing field

/** Initializes everything, enables interrupts and green LED,
 *  and handles the rendering for the screen
 */
void main() {
  P1DIR |= GREEN_LED;        // Green led on when CPU on
    P1OUT |= GREEN_LED;

    configureClocks();
    lcd_init();
    shapeInit();
    buzzer_init();
    p2sw_init(15);

    if (goal == 1) {
        buzzer_set_period(0);
    }

    shapeInit();
    layerInit(&layer0);
    layerDraw(&layer0);

    layerGetBounds(&fieldLayer, &fieldFence);

    enableWDTInterrupts();      /**< enable periodic interrupt */
    or_sr(0x8);                  /**< GIE (enable interrupts) */

    for (;;) {
        while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */
            P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
            or_sr(0x10);          /**< CPU OFF */
        }
        P1OUT |= GREEN_LED;
        /**< Green led on when CPU on */
        redrawScreen = 0;

        movLayerDraw(&ml1, &layer0);
        movLayerDraw(&ml2, &layer0);
        movLayerDraw(&ml3, &layer0);
    }
}

    //Watchdog timer interrupt handler. 15 interrupts/sec
    //SW1-SW2 will move the bottom paddle while SW3-SW4 will move the upper paddle
void wdt_c_handler() {
    static short count = 0;
    P1OUT |= GREEN_LED;              // Green LED on when cpu on
    static long wait = 0;
    count++;
    u_int sw = p2sw_read();

    //intro screen shows the controls for the next 5 seconds as well as a count down
    while (++wait < 185) {
        drawString5x7(screenWidth / 2 - 50, 30, "Welcome to Pong", COLOR_BLACK, COLOR_VIOLET);
        drawString5x7(15, 50, "S1: GOLD Left", COLOR_GOLD, COLOR_VIOLET);
        drawString5x7(15, 65, "S2: GOLD Right", COLOR_GOLD, COLOR_VIOLET);
        drawString5x7(15, 80, "S3: BLACK Left", COLOR_BLACK, COLOR_VIOLET);
        drawString5x7(15, 95, "S4: BLACK Right", COLOR_BLACK, COLOR_VIOLET);

        //3 second countdown
        if(wait < 50){
        drawChar5x7(screenWidth/2-2,110,'3', COLOR_BLACK,COLOR_VIOLET);
        }else if(wait < 100){
        drawChar5x7(screenWidth/2-2,110,'2', COLOR_BLACK,COLOR_VIOLET);
        }else if(wait < 150){
        drawChar5x7(screenWidth/2-2,110,'1', COLOR_BLACK,COLOR_VIOLET);
	} else{
	drawString5x7(screenWidth/2-2,110,"Begin!", COLOR_BLACK,COLOR_VIOLET);
	}
	
    }

    //after 5 second delay the screen is redrawn and ready for play
    //game begins when one of he paddles is moved
    if (wait == 200)
        layerDraw(&layer0);

    //win condition is met when either player reaches a score of 5
    //then restarts after a few seconds
    if (p1Score == 5 || p2Score == 5) {
        char *winner;
        char newGame = 1;
        (p1Score == 5) ? (winner = "Player1!") : (winner = "Player2!");
        bgColor = COLOR_WHITE;
        layerDraw(&layer0);
        drawString5x7(screenWidth / 2, screenHeight / 2, "Game Over", COLOR_VIOLET, COLOR_WHITE);
        drawString5x7(screenWidth / 2, screenHeight / 2 + 10, "WINNER", COLOR_VIOLET, COLOR_WHITE);
        drawString5x7(screenWidth / 2, screenHeight / 2 + 20, winner, COLOR_VIOLET, COLOR_WHITE);
        //song
	//gameOver(0);
        while (newGame) {
            if (sw) {
	            main();
            }
        }
    }

    if (count == 20) {
        mlAdvance(&ml1, &ml2, &ml3, &fieldFence);

        u_int switches = p2sw_read();

        //move btm paddle left
        if (!(switches & (1 << 1)))
        {
            if (ml2.layer->posNext.axes[0] <= 102) {
                ml2.layer->posNext.axes[0] += 5;

                redrawScreen = 1;
                goal = 0;
            }
        }
        //move btm paddle right
        if (!(switches & (1 << 0)))
        {
            if (ml2.layer->posNext.axes[0] >= 27) {
                ml2.layer->posNext.axes[0] -= 5;

                redrawScreen = 1;
                goal = 0;
            }
        }
        //move top paddle left
        if (!(switches & (1 << 2)))
        {
            if (ml3.layer->posNext.axes[0] >= 26) {
                ml3.layer->posNext.axes[0] -= 5;

                redrawScreen = 1;
                goal = 0;
            }
        }
        //move top paddle left
        if (!(switches & (1 << 3)))
        {
            if (ml3.layer->posNext.axes[0] <= 102) {
                ml3.layer->posNext.axes[0] += 5;

                redrawScreen = 1;
                goal = 0;
            }
        }

        redrawScreen = 1;
        count = 0;
    }
    P1OUT &= ~GREEN_LED;            // Green LED off when cpu off
}
