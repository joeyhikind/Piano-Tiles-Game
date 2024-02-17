
/* This files provides address values that exist in the system */
//Starting screen image array:

#include "image_matrix.h"

#define SDRAM_BASE            0xC0000000
#define FPGA_ONCHIP_BASE      0xC8000000
#define FPGA_CHAR_BASE        0xC9000000

/* Cyclone V FPGA devices */
#define LEDR_BASE             0xFF200000
	
#define HEX3_HEX0_BASE        0xFF200020
#define HEX5_HEX4_BASE        0xFF200030
#define SW_BASE               0xFF200040
#define KEY_BASE              0xFF200050
#define TIMER_BASE            0xFF202000
#define PIXEL_BUF_CTRL_BASE   0xFF203020
#define CHAR_BUF_CTRL_BASE    0xFF203030

/* VGA colors */
#define WHITE 0xFFFF
#define YELLOW 0xFFE0
#define RED 0xF800
#define GREEN 0x07E0
#define BLUE 0x001F
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define GREY 0xC618
#define PINK 0xFC18
#define ORANGE 0xFC00

#define ABS(x) (((x) > 0) ? (x) : -(x))

/* Screen size. */
#define RESOLUTION_X 320
#define RESOLUTION_Y 240

/* Constants for animation */
#define BOX_LEN 2
#define NUM_BOXES 8

#define FALSE 0
#define TRUE 1

//Included Libraries
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

//Declared globals for storing which pixel buffer is used:
volatile int pixel_buffer_start; // global variable
volatile int* status_register_ptr;

//Global function definition========================================================================================================================
void clear_screen();
void clear_screen2();
void draw_square(int x, int y, short int color);
void wait_for_vsync();
void buffer_set_up();
void drawBackGround();
void drawBox(int x, int y, short int color);
void delay(double number_of_seconds);
void display_healthbar_on_LED(int health);
void display_score_HEX(int game_score);
int HEX_decoder(int value);
void display_start();
void display_game_over();
void plot_text(int x, int y, short int text);
void clear_text();

//function definition for interrupts
void disable_A9_interrupts (void);
void set_A9_IRQ_stack (void);
void config_GIC (void);
void config_KEYs (void);
void enable_A9_interrupts (void);


//global isbuffer
bool is_bufferA=true;

//Global Variables==================================================================================================================================
int dim = 10; //box dimension
#define total_boxes_per_column 5 //in total the max numbers of boxes we can have on the screen in 20;
int color_array [7] = {0xEDD9, 0xDDDD, 0xBDDD,0xBF5B,0xCF57,0xE757,0xEEB7};//array of color bit codes
int speed = 1; //keep track of box movement speed --> initially set as 1

//Struct for keeping track of boxes
struct Boxes{
	volatile int y_location; 
	volatile int x_location; //updated dynamically
	volatile int draw; //1 if will draw, 0 if not draw
	volatile int colour; //colour of given box
}; 

//create global array to track boxes
//for column A
struct Boxes boxesA[total_boxes_per_column]; //Real boxes location
struct Boxes boxesAFront[total_boxes_per_column]; //Front buffer
struct Boxes boxesABack[total_boxes_per_column]; //Back Buffer	
//for column B
struct Boxes boxesB[total_boxes_per_column]; //Real boxes location
struct Boxes boxesBFront[total_boxes_per_column]; //Front buffer
struct Boxes boxesBBack[total_boxes_per_column]; //Back Buffer	
//for column C
struct Boxes boxesC[total_boxes_per_column]; //Real boxes location
struct Boxes boxesCFront[total_boxes_per_column]; //Front buffer
struct Boxes boxesCBack[total_boxes_per_column]; //Back Buffer	
//for column D
struct Boxes boxesD[total_boxes_per_column]; //Real boxes location
struct Boxes boxesDFront[total_boxes_per_column]; //Front buffer
struct Boxes boxesDBack[total_boxes_per_column]; //Back Buffer	

//col num boxes tracker
int col_A_Boxes=0;
int col_B_Boxes=0;
int col_C_Boxes=0;
int col_D_Boxes=0;

//game score tracker
int score=0;
int health_bar=10; //player is allowed to miss 10 tiles
//==================================================================================================================================


int main(void)
{	

	//BUFFER SET UP =================================================================================================================================
	//random seed 
	srand(time(NULL));
	//pixel controller ptr
    volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
	
	//declare other variables:
	//status register ptr
	status_register_ptr= (int *)0xFF20302c;

    /* set front pixel buffer to start of FPGA On-chip memory */
    *(pixel_ctrl_ptr + 1) = 0xC8000000; // first store the address in the 
                                        // back buffer
    /* now, swap the front/back buffers, to set the front buffer location */
    wait_for_vsync();
    /* initialize a pointer to the pixel buffer, used by drawing functions */
    pixel_buffer_start = *pixel_ctrl_ptr;
    clear_screen(); // pixel_buffer_start points to the pixel buffer
	clear_text();//clear text

	//draw background in front buffer
	display_start();

    /* set back pixel buffer to start of SDRAM memory */
    *(pixel_ctrl_ptr + 1) = 0xC0000000;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer
    clear_screen(); // pixel_buffer_start points to the pixel buffer
	clear_text();

	//draw background in back buffer
	drawBackGround();
	
	//Game Start Page================================================================================================================================
	//Poll for key press to begin
	//ask for key press
	printf("Press any of the four keys to begin.... \n");
	//Polling.... 
	//address of Edge Capturer Register
	volatile int* KEY_EDGE_CAP = (int*)(KEY_BASE + 0xC);
	//reset KEYs to begin with
	*KEY_EDGE_CAP= 0b1111;
	while(1){
		if((*KEY_EDGE_CAP & 0xf)!=0){
			//reset edge Capturer register
			*KEY_EDGE_CAP = 0b1111;
			printf("%d",*KEY_EDGE_CAP);
			break;	
		}
	}

	//draw background in back buffer
	pixel_buffer_start = *pixel_ctrl_ptr;
    clear_screen(); // pixel_buffer_start points to the pixel buffer
	clear_text();//clear text
	drawBackGround();

	//Print game Instructions:
	printf("Welcome to Piano Tiles!!! \n");
	printf("There are four columns on screen where various colored piano tiles drop from top to bottom \n");
	printf("Your Task is to press the four keys corresponding to each of the columns right when each tile hits the bar drawn at the bottom ofthe screen \n");
	printf("Have FUN :)");

	//Interrupt Routine Set Up: =====================================================================================================================
	disable_A9_interrupts (); // disable interrupts in the A9 processor
	set_A9_IRQ_stack (); // initialize the stack pointer for IRQ mode
	config_GIC (); // configure the general interrupt controller
	config_KEYs (); // configure KEYs to generate interrupts
	enable_A9_interrupts (); // enable interrupts in the A9 processor

	//GAME SET UP ===================================================================================================================================
	//Create array of boxes of type Struct to track real movement
	//for loop iteration for initailization
	int i;
	for(i=0;i<total_boxes_per_column;i++){
		boxesA[i].draw=0;
		boxesB[i].draw=0;
		boxesC[i].draw=0;
		boxesD[i].draw=0;
		
		boxesA[i].y_location=0;
		boxesB[i].y_location=0;
		boxesC[i].y_location=0;
		boxesD[i].y_location=0;

		boxesAFront[i].draw=0;
		boxesBFront[i].draw=0;
		boxesCFront[i].draw=0;
		boxesDFront[i].draw=0;
			
		boxesABack[i].draw=0;
		boxesBBack[i].draw=0;
		boxesCBack[i].draw=0;
		boxesDBack[i].draw=0;

		//initialize colour for each box
		boxesA[i].colour=color_array[(rand () % 7)];	
		boxesB[i].colour=color_array[(rand () % 7)];	
		boxesC[i].colour=color_array[(rand () % 7)];	
		boxesD[i].colour=color_array[(rand () % 7)];

		//set colour as black
		boxesAFront[i].colour=0;
		boxesBFront[i].colour=0;
		boxesCFront[i].colour=0;
		boxesDFront[i].colour=0;

		boxesABack[i].colour=0;
		boxesBBack[i].colour=0;
		boxesCBack[i].colour=0;
		boxesDBack[i].colour=0;
	}
	
	
	//counters
	int iteration_counter=0;

	//display health bar on LED
	display_healthbar_on_LED(health_bar);
	
	//GAME START ====================================================================================================================================
    while (1)
    {	
		
		//print score and health
		//printf("score: %d \n",score);
		//printf("health: %d \n" , health_bar);

		//display score on HEX
		display_score_HEX(score);

		//Accounting for sum of boxes
		//int sumBoxes=col_A_Boxes+col_B_Boxes+ col_C_Boxes +col_D_Boxes;

		if(health_bar==0){
			printf("Game Over! \n");
			printf("Your score: %d \n",score);

			break;
		}
		
		//SCREEN ERASE:==============================================================================================================================
        if(pixel_buffer_start == 0xC8000000){ //erasing for back buffer
			int i;	
            for(i=0 ; i<5; i++){
                //for A
                if(boxesABack[i].draw==1){
                    drawBox(29,boxesABack[i].y_location, boxesABack[i].colour);
                    //reset draw and location to 0
                    boxesABack[i].y_location=0;
                    boxesABack[i].draw=0;
                }
                
                //for B
                if(boxesBBack[i].draw==1){
                    drawBox(89,boxesBBack[i].y_location, boxesBBack[i].colour);
                    //reset draw and location to 0
                    boxesBBack[i].y_location=0;
                    boxesBBack[i].draw=0;
                }

                //for C
                if(boxesCBack[i].draw==1){
                    drawBox(149,boxesCBack[i].y_location, boxesCBack[i].colour);
                    //reset draw and location to 0
                    boxesCBack[i].y_location=0;
                    boxesCBack[i].draw=0;
                }                

                //for D
                if(boxesDBack[i].draw==1){
                    drawBox(209,boxesDBack[i].y_location, boxesDBack[i].colour);
                    //reset draw and location to 0
                    boxesDBack[i].y_location=0;
                    boxesDBack[i].draw=0;
                }     
            
            }
        }
        else{ //erasing for front buffer
			int i;
            for(i=0 ; i<5; i++){
                //for A
                if(boxesAFront[i].draw==1){
                    drawBox(29,boxesAFront[i].y_location, boxesAFront[i].colour);
                    //reset draw and location to 0
                    boxesAFront[i].y_location=0;
                    boxesAFront[i].draw=0;
                }
                
                //for B
                if(boxesBFront[i].draw==1){
                    drawBox(89,boxesBFront[i].y_location, boxesBFront[i].colour);
                    //reset draw and location to 0
                    boxesBFront[i].y_location=0;
                    boxesBFront[i].draw=0;
                }

                //for C
                if(boxesCFront[i].draw==1){
                    drawBox(149,boxesCFront[i].y_location, boxesCFront[i].colour);
                    //reset draw and location to 0
                    boxesCFront[i].y_location=0;
                    boxesCFront[i].draw=0;
                }                

                //for D
                if(boxesDFront[i].draw==1){
                    drawBox(209,boxesDFront[i].y_location, boxesDFront[i].colour);
                    //reset draw and location to 0
                    boxesDFront[i].y_location=0;
                    boxesDFront[i].draw=0;
                }   
            }
        }

		//RANDOMIZE BOX DROP:========================================================================================================================
		//happens everytime a line of boxes travel 10 units of screen pixel
		if(iteration_counter==20){
			//reset counter
			iteration_counter=0;
			if((col_A_Boxes+col_B_Boxes+ col_C_Boxes +col_D_Boxes)!=20){
				if(col_A_Boxes!=5){
					int draw_it=(rand () % 2);
					if(draw_it==1){
						//iterate col of boxes --> decremented when erasing
						int i;
						for(i=0; i<5; i++){
							if(boxesA[i].draw != 1){
								break;
							}
						}
						col_A_Boxes++;
						boxesA[i].draw=draw_it; //set draw as 1 depending on random generator
					}
				}

				if(col_B_Boxes!=5){
					int draw_it=(rand () % 2);
					if(draw_it==1){
						//iterate col of boxes --> decremented when erasing
						int i;
						for(i=0; i<5; i++){
							if(boxesB[i].draw != 1){
								break;
							}
						}
						col_B_Boxes++;
						boxesB[i].draw=draw_it; //set draw as 1 depending on random generator						
					}
				}

				if(col_C_Boxes!=5){
					int draw_it=(rand () % 2);
					if(draw_it==1){
						//iterate col of boxes --> decremented when erasing
						int i;
						for(i=0; i<5; i++){
							if(boxesC[i].draw != 1){
								break;
							}
						}
						col_C_Boxes++;
						boxesC[i].draw=draw_it; //set draw as 1 depending on random generator
					}
				}		

				if(col_D_Boxes!=5){
					int draw_it=(rand () % 2);
					if(draw_it==1){
						//iterate col of boxes --> decremented when erasing
						int i;
						for(i=0; i<5; i++){
							if(boxesD[i].draw != 1){
								break;
							}
						}
						col_D_Boxes++;
						boxesD[i].draw=draw_it; //set draw as 1 depending on random generator
					}
				}		
			}
		}

		//DRAW:====================================================================================================================================
		//drawing part only has access to parts of the screen from x[1 to 240] and y[1 to 200]
		int i;
		for(i=0; i<5; i++){
			if(boxesA[i].draw == 1){
				drawBox(29, boxesA[i].y_location,boxesA[i].colour);
			}
			if(boxesB[i].draw == 1){
				drawBox(89, boxesB[i].y_location,boxesB[i].colour);
			}
			if(boxesC[i].draw == 1){
				drawBox(149, boxesC[i].y_location,boxesC[i].colour);
			}	
			if(boxesD[i].draw == 1){
				drawBox(209, boxesD[i].y_location,boxesD[i].colour);
			}		
		}
		

		//UPDATE:====================================================================================================================================
		int i1;
		for(i1=0 ;i1<5; i1++){
			if(boxesA[i1].draw==1){
                //for buffers
                if(pixel_buffer_start == 0xC8000000){ //back buffer
                    boxesABack[i1].draw=1; 
                    boxesABack[i1].y_location=boxesA[i1].y_location;
                }
                else{
                    boxesAFront[i1].draw=1;
                    boxesAFront[i1].y_location=boxesA[i1].y_location;
                }

                //After storing in buffers array, update new location
				//if box reached end of screen
				if(boxesA[i1].y_location==220){
					boxesA[i1].draw=0; //if given box reaches the end of screen--> set boolean draw to 0 and reset its locations
					boxesA[i1].y_location=0;
					//decrement col count
					col_A_Boxes--;

					//decrement life count
					if(health_bar>0){
						health_bar--;
					}
				}
				else{
					boxesA[i1].y_location=boxesA[i1].y_location+speed;
				}
			}

			if(boxesB[i1].draw==1){
                //for buffers
                if(pixel_buffer_start == 0xC8000000){ //back buffer
                    boxesBBack[i1].draw=1; 
                    boxesBBack[i1].y_location=boxesB[i1].y_location;
                }
                else{
                    boxesBFront[i1].draw=1;
                    boxesBFront[i1].y_location=boxesB[i1].y_location;
                }

                //After storing in buffers array, update new location
				//if box reached end of screen
				if(boxesB[i1].y_location==220){
					boxesB[i1].draw=0; //if given box reaches the end of screen--> set boolean draw to 0 and reset its locations
					boxesB[i1].y_location=0;
					col_B_Boxes--;
					
					//decrement life count
					if(health_bar>0){
						health_bar--;
					}
				}
				else{
					boxesB[i1].y_location=boxesB[i1].y_location+speed;
				}
			}

			if(boxesC[i1].draw==1){
                //for buffers
                if(pixel_buffer_start == 0xC8000000){ //back buffer
                    boxesCBack[i1].draw=1; 
                    boxesCBack[i1].y_location=boxesC[i1].y_location;
                }
                else{
                    boxesCFront[i1].draw=1;
                    boxesCFront[i1].y_location=boxesC[i1].y_location;
                }
                //After storing in buffers array, update new location
				//if box reached end of screen
				if(boxesC[i1].y_location==220){
					boxesC[i1].draw=0; //if given box reaches the end of screen--> set boolean draw to 0 and reset its locations
					boxesC[i1].y_location=0;
					col_C_Boxes--;
					//decrement life count
					if(health_bar>0){
						health_bar--;
					}
				}
				else{
					boxesC[i1].y_location=boxesC[i1].y_location+speed;
				}
			}

			if(boxesD[i1].draw==1){
                //for buffers
                if(pixel_buffer_start == 0xC8000000){ //back buffer
                    boxesDBack[i1].draw=1; 
                    boxesDBack[i1].y_location=boxesD[i1].y_location;
                }
                else{
                    boxesDFront[i1].draw=1;
                    boxesDFront[i1].y_location=boxesD[i1].y_location;
                }

                //After storing in buffers array, update new location
				//if box reached end of screen
				if(boxesD[i1].y_location==220){
					boxesD[i1].draw=0; //if given box reaches the end of screen--> set boolean draw to 0 and reset its locations
					boxesD[i1].y_location=0;
					col_D_Boxes--;
					//decrement life count
					if(health_bar>0){
						health_bar--;
					}
				}
				else{
					boxesD[i1].y_location=boxesD[i1].y_location+speed;
				}
			}
				
		}
		//display health bar on LED
		display_healthbar_on_LED(health_bar);


		//SWITCH BUffer:=============================================================================================================================
		//My part ends here
       	wait_for_vsync(); // swap front and back buffers on VGA vertical sync
        pixel_buffer_start = *(pixel_ctrl_ptr + 1); // new back buffer
		iteration_counter++; //update to keep track of iterations
		//printf("%d \n",sumBoxes);
		//delay(0.01);
	}

	display_game_over(); //display game_over text
	
	/* set front pixel buffer to start of FPGA On-chip memory */
    *(pixel_ctrl_ptr + 1) = 0xC8000000; // first store the address in the 
                                        // back buffer
    /* now, swap the front/back buffers, to set the front buffer location */
    wait_for_vsync();
    /* initialize a pointer to the pixel buffer, used by drawing functions */
    pixel_buffer_start = *pixel_ctrl_ptr;
    clear_screen(); // pixel_buffer_start points to the pixel buffer

    /* set back pixel buffer to start of SDRAM memory */
    *(pixel_ctrl_ptr + 1) = 0xC0000000;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer
    clear_screen(); // pixel_buffer_start points to the pixel buffer

}


// code for subroutines:

//wait_for_vsync subroutine
void wait_for_vsync(){
		//synchronize by writing 1 into front buffer
		int *pixel_ctrl_ptr= (int *) 0xFF203020;
		*pixel_ctrl_ptr=1;
		
		//poll for when swap happened
		while(1){
			if((*status_register_ptr & 0b1)==0)
				break;
		}
}
//swap function to switch values of two variables: pass by reference!
void swap (int *a, int *b){
	int temp= *b;
	*b= *a;
	*a= temp;
}

//draw pixel given x and y location and line_color in binary
void plot_pixel(int x, int y, short int line_color)
{
    *(short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = line_color;
}

//draw line function 
void draw_line(int x0, int y0, int x1, int y1, short int color){
	//initialization ad adjustments
	bool is_steep=abs(y1-y0) > abs(x1-x0);
	//determine line steepness
	//int d_y= abs(x1-x0);
	//int d_x= abs(y1-y0);
	//determine if swap is needed to adjust for steep lines
	if(is_steep){
		swap(&x0,&y0);
		swap(&x1,&y1);
		//set is_steep to true
		//is_steep=TRUE;
	}
	if(x0>x1){
		swap(&x0,&x1);
		swap(&y0,&y1);
	}
	//error initialization
	double delta_x= x1-x0;
	double delta_y= ABS(y1-y0);
	int y_step; //y_step initialized
	
	//error
	int error = -(delta_x / 2);
	
	//y begins at y0
	int y= y0;
	
	//dtermine direction of step
	if(y0<y1)
		y_step=1;
	else
		y_step=-1;
	
	//Line algorithm:
	int i;
	for(i=x0; i<=x1; i++){ //iteration of x up one by one until x reaches x1
		//case for steep line
		if(is_steep){
			plot_pixel(y, i, color);	
		}
		
		//otherwise...
		else{
			plot_pixel(i,y,color);
		}
		
		//set error
			error=error+delta_y;

		//determine y iteration based on error
		if(error>0){
			y=y+y_step;
			error=error-delta_x;
		}		
	}
}

//clear_screen() sub routine
void clear_screen(){
	//create two for loops to write 0 into every pixel from x=0 to x=319 and y=0 to y=239
	int i;
	for(i=0; i<320; i++){
		int j;
		for(j=0; j<240; j++){
			plot_pixel(i,j,0); //color here is set to zero to make screen blank	
		}
	}
}

//clear_screen() sub routine
void clear_screen2(){
	//create two for loops to write 0 into every pixel from x=0 to x=319 and y=0 to y=239
	int i;
	for(i=0; i<240; i++){
		int j;
		for(j=0; j<200; j++){
			plot_pixel(i,j,0); //color here is set to zero to make screen blank	
		}
	}
}

//clear text buffer
void clear_text(){
	int i;
	for (i = 38; i < 42; i++){
		int j;
		for (j = 29; j < 32; j++){
			plot_text(i,j, 32);
		}
	}
}

//Helper Functions:

//drawBackground Helper function
void drawBackGround(){
	//setup background
	draw_line(0, 200, 270, 200, 0xFFFF);
	draw_line(270, 0, 270, 240, 0xFFFF);
	draw_line(0, 220,270,220, 0xFFFF);
	
	//drawing piano on right
	int i;
	for (i = 0; i < 240; i++){//draw white tiles
		short int color = 0x0000;
		if (i % 20 != 0 && color == 0xFFFF){
			color = 0x0000;
		}else if (i % 20 != 0 && color == 0x0000){
			color = 0xFFFF;
		}
		draw_line(271,i,320,i, color);
	}
	int i2;
	for (i2 = 0; i2 < 12; i2++){
		draw_line(271,i2*20,305,i2*20, 0x0000);
		draw_line(271,i2*20+1,305,i2*20+1, 0x0000);
		draw_line(271,i2*20+2,305,i2*20+2, 0x0000);
		draw_line(271,i2*20+3,305,i2*20+3, 0x0000);
		draw_line(271,i2*20+4,305,i2*20+4, 0x0000);
		draw_line(271,i2*20+5,305,i2*20+5, 0x0000);
	}
}

//draw box helper function
void drawBox(int x, int y, short int color){
	int dimx=3*dim;
	int j;
	for (j = 0; j < dim; j++){
		int i;
		for (i = 0; i < dimx; i++){
			if (i == dimx/2 && j == dim/2){//middle case
				plot_pixel(x+i,y+j, 0xF800);
			}else{
				plot_pixel(x+i,y+j,color);
			}
		}
	}
}

//temporary delay function 
void delay(double number_of_seconds){
	// Converting time into milli_seconds
    int milli_seconds = 1000 * number_of_seconds;
  
    // Storing start time
    clock_t start_time = clock();
  
    // looping till required time is not achieved
    while (clock() < start_time + milli_seconds)
        ;
}

//function for displaying Health bar on LED
void display_healthbar_on_LED(int health){
	int* LED_ptr= (int *) LEDR_BASE;
	int value= health;
	switch(value) {
  		case (0):
			//display nothing
			*LED_ptr=0;
    		break;

		case (1):
			//display 1
			*LED_ptr=0b0000000001;
			break;

		case (2):
			//display 1
			*LED_ptr=0b0000000011;
			break;

		case (3):
			//display 1
			*LED_ptr=0b0000000111;
			break;

		case (4):
			//display 1
			*LED_ptr=0b0000001111;
			break;

		case (5):
			//display 1
			*LED_ptr=0b0000011111;
			break;

		case (6):
			//display 1
			*LED_ptr=0b0000111111;
			break;

		case (7):
			//display 1
			*LED_ptr=0b0001111111;
			break;

		case (8):
			//display 1
			*LED_ptr=0b0011111111;
			break;

		case (9):
			//display 1
			*LED_ptr=0b0111111111;
			break;

		case (10):
			//display 1
			*LED_ptr=0b1111111111;
			break;

    // code block
	}
}

//function for displaying score on HEX display
void display_score_HEX(int game_score){
	//isolate game_score into digits
	//temporary number holder
	int temp=game_score;
	//thousands 
	int thousands=0;
	int hundreds=0;
	int tens=0;
	int digits=0;
	if(game_score>=1000){
		while(temp!=0){
			temp=temp/1000;
			thousands++;
		}
	}

	//hundreds
	temp=game_score;
	if(game_score>=100){
		while(temp!=0){
			temp=temp/100;
			hundreds++;
		}
	}

	//tens
	temp=game_score;
	if(game_score>=10){
		while(temp!=0){
			temp=temp/100;
			tens++;
		}
	}

	//digits
	temp=game_score;
	digits=temp%10;

	//encode these digits into HEX display
	int hex_thousand= HEX_decoder(thousands)<<24;
	int hex_hundred= HEX_decoder(hundreds)<<16;
	int hex_tens= HEX_decoder(tens)<<8;
	int hex_digit=HEX_decoder(digits);

	//add these numbers up for complete result
	int total_hex_display= hex_thousand+ hex_hundred+hex_tens+hex_digit;

	//store in hex
	int * pointer_to_hex=(int *) 0xFF200020;
	*pointer_to_hex=total_hex_display;
}

int HEX_decoder(int value){
	int array[10]= {0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110,0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01100111};
	return array[value];
}

//draw into character buffer
void plot_text(int x, int y, short int text){
	*(short int *)(0xC9000000 + (y<<7) + (x)) = text;
}
//displays game over screen
void display_game_over(){
	plot_text(38,29, 71);
	plot_text(39,29, 97);
	plot_text(40,29, 109);
	plot_text(41,29, 101);
	plot_text(38,30, 79);
	plot_text(39,30, 118);
	plot_text(40,30, 101);
	plot_text(41,30, 114);
	plot_text(39,31, 58);
	plot_text(40,31, 40);
}
//displays start game screen
void display_start(){
	int i;
	for (i = 0; i < 240; i++){
		int j;
		for (j = 0; j < 320; j++){
			plot_pixel(j, i, pianoTiles[i][j]);
		}
	}
}
//Code for Interrupt request=======================================================================================================================================
/* setup the KEY interrupts in the FPGA */
void config_KEYs()
{
	volatile int * KEY_ptr = (int *) 0xFF200050; // KEY base address
	*(KEY_ptr + 2) = 0xF; // enable interrupts for all four KEYs
}


void pushbutton_ISR (void);
void config_interrupt (int, int);
// Define the IRQ exception handler
void __attribute__ ((interrupt)) __cs3_isr_irq (void){
	// Read the ICCIAR from the CPU Interface in the GIC
	int interrupt_ID = *((int *) 0xFFFEC10C);
	if (interrupt_ID == 73) // check if interrupt is from the KEYs
		pushbutton_ISR ();
	else
		while (1); // if unexpected, then stay here
	// Write to the End of Interrupt Register (ICCEOIR)
	*((int *) 0xFFFEC110) = interrupt_ID;
}
// Define the remaining exception handlers
void __attribute__ ((interrupt)) __cs3_reset (void){
	while(1);
}
void __attribute__ ((interrupt)) __cs3_isr_undef (void){
	while(1);
}
void __attribute__ ((interrupt)) __cs3_isr_swi (void){
	while(1);
}
void __attribute__ ((interrupt)) __cs3_isr_pabort (void){
	while(1);
}
void __attribute__ ((interrupt)) __cs3_isr_dabort (void)
{
while(1);
}
void __attribute__ ((interrupt)) __cs3_isr_fiq (void)
{
while(1);
}
/*
* Turn off interrupts in the ARM processor
*/
void disable_A9_interrupts(void){
	int status = 0b11010011;
	asm("msr cpsr, %[ps]" : : [ps]"r"(status));
}
/*
* Initialize the banked stack pointer register for IRQ mode
*/
void set_A9_IRQ_stack(void){
	int stack, mode;
	stack = 0xFFFFFFFF - 7; // top of A9 onchip memory, aligned to 8 bytes
	/* change processor to IRQ mode with interrupts disabled */
	mode = 0b11010010;
	asm("msr cpsr, %[ps]" : : [ps] "r" (mode));
	/* set banked stack pointer */
	asm("mov sp, %[ps]" : : [ps] "r" (stack));
	/* go back to SVC mode before executing subroutine return! */
	mode = 0b11010011;
	asm("msr cpsr, %[ps]" : : [ps] "r" (mode));
}
/*
* Turn on interrupts in the ARM processor
*/
void enable_A9_interrupts(void){
	int status = 0b01010011;
	asm("msr cpsr, %[ps]" : : [ps]"r"(status));
}

/*
* Configure the Generic Interrupt Controller (GIC)
*/
void config_GIC(void){
	config_interrupt (73, 1); // configure the FPGA KEYs interrupt (73)
	// Set Interrupt Priority Mask Register (ICCPMR). Enable all priorities
	*((int *) 0xFFFEC104) = 0xFFFF;
	// Set the enable in the CPU Interface Control Register (ICCICR)
	*((int *) 0xFFFEC100) = 1;
	// Set the enable in the Distributor Control Register (ICDDCR)
	*((int *) 0xFFFED000) = 1;
}
void config_interrupt (int N, int CPU_target){
	int reg_offset, index, value, address;
	/* Configure the Interrupt Set-Enable Registers (ICDISERn).
	* reg_offset = (integer_div(N / 32) * 4; value = 1 << (N mod 32) */
	reg_offset = (N >> 3) & 0xFFFFFFFC;
	index = N & 0x1F;
	value = 0x1 << index;
	address = 0xFFFED100 + reg_offset;
	/* Using the address and value, set the appropriate bit */
	*(int *)address |= value;
	/* Configure the Interrupt Processor Targets Register (ICDIPTRn)
	* reg_offset = integer_div(N / 4) * 4; index = N mod 4 */
	reg_offset = (N & 0xFFFFFFFC);
	index = N & 0x3;
	address = 0xFFFED800 + reg_offset + index;
	/* Using the address and value, write to (only) the appropriate byte */
	*(char *)address = (char) CPU_target;
}

/********************************************************************
* Pushbutton - Interrupt Service Routine
*
* This routine checks which KEY has been pressed. It writes to HEX0
*******************************************************************/
void pushbutton_ISR( void )
{
	/* KEY base address */
	volatile int *KEY_ptr = (int *) 0xFF200050;
	/* HEX display base address */
	//volatile int *HEX3_HEX0_ptr = (int *) 0xFF200020;
	int press;
	press = *(KEY_ptr + 3); // read the pushbutton interrupt register
	*(KEY_ptr + 3) = press; // Clear the interrupt

	if (press & 0x1){// KEY0
		//HEX_bits = 0b00111111;

		//check key press context
		int i;
		for(i=0; i<total_boxes_per_column;i++){
			if(boxesD[i].draw==1){
				if(boxesD[i].y_location >= 200 && boxesD[i].y_location <= 220){
					//iterate score 
					score++;
					//make box disappear
					boxesD[i].draw=0;
					boxesD[i].y_location=0;
					col_D_Boxes--; //decrement column box tracker
				}
			}
		}
	}

	else if (press & 0x2){ // KEY1
		//HEX_bits = 0b00000110;
		//check key press context
		int i;
		for(i=0; i<total_boxes_per_column;i++){
			if(boxesC[i].draw==1){
				if(boxesC[i].y_location >= 200 && boxesC[i].y_location <= 220){
					//iterate score 
					score++;
					//make box disappear
					boxesC[i].draw=0;
					boxesC[i].y_location=0;
					col_C_Boxes--; //decrement column box tracker
				}
			}
		}
	}
	else if (press & 0x4){ // KEY2
		//HEX_bits = 0b01011011;

		//check key press context
		int i;
		for(i=0; i<total_boxes_per_column;i++){
			if(boxesB[i].draw==1){
				if(boxesB[i].y_location >= 200 && boxesB[i].y_location <= 220){
					//iterate score 
					score++;
					//make box disappear
					boxesB[i].draw=0;
					boxesB[i].y_location=0;
					col_B_Boxes--; //decrement column box tracker
				}
			}
		}
	}

	else {// press & 0x8, which is KEY3
		//HEX_bits = 0b01001111;

		//check key press context
		int i;
		for(i=0; i<total_boxes_per_column;i++){
			if(boxesA[i].draw==1){
				if(boxesA[i].y_location >= 200 && boxesA[i].y_location <= 220){
					//iterate score 
					score++;
					//make box disappear
					boxesA[i].draw=0;
					boxesA[i].y_location=0;
					col_A_Boxes--; //decrement column box tracker
				}
			}
		}
	}
	//*HEX3_HEX0_ptr = HEX_bits;
	return;
}