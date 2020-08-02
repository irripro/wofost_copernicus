#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include "wofost.h"
#include "extern.h"


int main(int argc, char **argv)
{
    
    FILE **output;
    
    SimUnit *initial  = NULL;
    Weather *head;
      
    int CycleLength   = 300;
    int NumberOfFiles;
    int Emergence;
    
    char list[MAX_STRING];
    char meteolist[MAX_STRING];
    char name[MAX_STRING];
    
    Step = 1.;
    
    if (argc != 3) exit(0);
    if (strlen(argv[1]) >= MAX_STRING) exit(0);
    if (strlen(argv[2]) >= MAX_STRING) exit(0);
    
    memset(list,'\0',MAX_STRING);
    memset(meteolist,'\0',MAX_STRING);
    
    strncpy(list,argv[1],strlen(argv[1]));
    strncpy(meteolist,argv[2],strlen(argv[2]));
    
    /* Fill the crop, soil, site and management place holders*/
    NumberOfFiles = GetSimInput(list);
    
    /* Set the initial Grid address */
    initial = Grid;    
    
    /* Get the meteo filenames and put them in the placeholder */
    GetMeteoInput(meteolist);
    
    /* Allocate memory for the file pointers */
    output = malloc(sizeof(**output) * NumberOfFiles);
    
    /* Go back to the beginning of the list */
    Grid = initial;
    
    /* Open the output files */
    while (Grid)
    {   /* Make valgrind happy  */
        memset(name,'\0',MAX_STRING);
        strncpy(name, Grid->output,strlen(Grid->output));
           
        output[Grid->file] = fopen(name, "w");
        header(output[Grid->file]);
        Grid = Grid->next;
    }
    
    /* Go back to the beginning of the list */
    Grid = initial;
    
    
    while (Meteo)
    {
        /* Get the meteodata */
        GetMeteoData(Meteo->path, Meteo->model);
        printf("%s\n", Meteo->model);
        
        for (lat = 0; lat < lat_length; lat++)
        {  
            for (lon = 0; lon < lon_length; lon++)
            {  
                 for (Day = 1; Day < time_length; Day++) //assume that the series start January first
                  {                   
                    /* Go back to the beginning of the list */
                    Grid = initial;

                    /* Set the date struct */
                    memset(&current_date, 0, sizeof(current_date)); 
                    current_date.tm_year = Meteo->Initial -1900;
                    current_date.tm_mday =  0 + Day;
                    mktime(&current_date);

                    while (Grid)
                    {
                        /* Get data, states and rates from the Grid structure and */
                        /* put them in the place holders */
                        Crop      = Grid->crp;
                        WatBal    = Grid->soil;
                        Mng       = Grid->mng;
                        Site      = Grid->ste;

                        Emergence = Grid->emergence; /* Start simulation at sowing or emergence */

                        Temp = 0.5 * (Tmax[Day][lat][lon] + Tmin[Day][lat][lon]);

                        /* Only simulate between start and end year */
                        if ( (current_date.tm_year + 1900) >=  Meteo->StartYear && 
                             (current_date.tm_year + 1900) <= Meteo->EndYear + 1)
                        {   
                            /* Determine if the sowing already has occurred */
                            IfSowing(Grid->start);

                            /* If sowing gas occurred than determine the emergence */
                            if (Crop->Sowing >= 1 && Crop->Emergence == 0)
                            {
                                if (EmergenceCrop(Emergence))
                                {                
                                    /* Initialize: set state variables */
                                    InitializeCrop();
                                    InitializeWatBal();
                                    InitializeNutrients(); 
                                }  
                            }

                            if (Crop->Sowing >= 1 && Crop->Emergence == 1)
                            {   
                                if (Crop->st.Development <= (Crop->prm.DevelopStageHarvest) 
                                        && Crop->GrowthDay < CycleLength) 
                                {
                                    Astro();
                                    CalcPenman();

                                   /* Calculate the evapotranspiration */
                                    EvapTra();

                                    /* Set the rate variables to zero */
                                    RatesToZero();

                                     /* Rate calculations */
                                    RateCalulationWatBal();
                                    Partioning();
                                    RateCalcultionNutrients();
                                    RateCalculationCrop();

                                    /* Write to the output files */
                                    //Output(output[Grid->file]);   

                                    /* Calculate LAI */
                                    Crop->st.LAI = LeaveAreaIndex(); 
                                    
                                    /* Establish TLAI*/
                                    if (Crop->st.LAI > Crop->st.TLAI)
                                        Crop->st.TLAI = Crop->st.LAI;
                                    
                                    /* State calculations */
                                    IntegrationCrop();
                                    IntegrationWatBal();
                                    IntegrationNutrients();

                                    /* Update the number of days that the crop has grown*/
                                    Crop->GrowthDay++;
                                }
                                else
                                {
                                    /* Write to the output files */
                                    Output(output[Grid->file]); 
                                    //printf("%5.2f %5.2f %5d\n",lats[lat], lons[lon], Crop->GrowthDay);
                                    Crop->TSumEmergence = 0;
                                    Crop->Emergence = 0;
                                    Crop->Sowing    = 0;
                                }
                            }
                        }    

                        /* Store the daily calculations in the Grid structure */
                        Grid->crp  = Crop;
                        Grid->soil = WatBal;
                        Grid->mng  = Mng;
                        Grid->ste  = Site;
                        Grid = Grid->next;
                    }
                } 
            } 
        }
        
    head = Meteo;
    Meteo = Meteo->next;
    free(head);
    }
    free(Meteo);
    
    /* Return to the beginning of the list */
    Grid = initial;
    
    /* Close the output files and free the allocated memory */
    while(Grid)
    {
        fclose(output[Grid->file]);
        Grid = Grid->next;
    }
    free(output);

    /* Go back to the beginning of the list */
    Grid = initial;
    Clean(Grid);

    return 1;
}
