        // Based on FW 12.0.1
        // Commands:
        // 
        // $PROCEED [Send at startup to get past the bootloader]
        //
        // $FACT [Factory preset]
        // $GETBDELAY [returns board delay, nanoseconds]
        // $GETDELAY [returns cable delay, nanoseconds]
        // $GETPOS [return position ]
        // $GETSCALEFACTOR [Returns a float, such as 3.742106e-3. Value is crystal's gain in Hz/mV.] 
        // $GETVER [returns version info]
        // $KALDBG <0|1> [Enable reporting of Kalman filter parameters]
        // $PPSDBG <0|1> [Enable or disable timing information every second]
        // $RESET [Unit software reset]
        // $SETBDELAY <n> [-32 <= n <= 32, Set board delay, PPS4 units (roughly 5 or 6 ns). PPS4 is controlled to equal this value]
				//
        // $SETDELAY <n> [-9999 <= n <= +9999 Set cable delay, 0.1us unit (100ns)]
				// Writes 1PPS correction to furuno using $PFEC,GPset,t-%06d
				//
        // $SETPOS <n> <n> <n> [set position to Lat/Long/Elevation_MSL, send value returned by survey]
        // $SURVEY <n> [survey for n hours, default is 8, 1<=n<=49]
        // $TRAINOXCO [Start OXCO Training. This restarts the board ($PROCEED needed), and measures freq
        //			    change with 500 ADC count. See result with $GETSCALINGFACTOR.]
				//			    this executes $FACT command.
        //
        // Other unknown commands:
        // $GETA [returns -1; Attenuator?]
        // $GETP [returns -1 255; Potentiometer?]
				//
        // $SET1PPS x y
				// This formats GPrrs message for Furuno - $PFEC,GPrrs,x,100,y
				// x - 0 = traim off, 1 = traim on
				// y - 0-nopps output, 1-always output pps, 2-output in ordinary way, 3 - output only when sufficient satellites are tracked
				// Internally they use "$SET1PPS 1 1" and "$SET1PPS 1 3"
				//
        // $SETGAIN <n> [Sets gain Parameter, n is integer, in hundreths. Maybe a multiplication factor for the DAC adjustment.]
        //
        // $ESN xxx
        // programs serial number to xxx
        //
        // $REPORT [x [y [z]]]
				// set reporting time (optional)
				// x - time report period (in sec), this controls $CLOCK message
				// y - sat report (on/off), control sending of $SAT, $WSAT
				// z - state report period (in sec), $STATUS and $EXTSTATUS
				// reports all SAT and WSAT messages stored in mcu if reporting enabled
        //
        // $SETINTG num
        // sets integrator ?
        //
        // $SETOFFSET num
        // sets DAC offset - this is not persistent, and will not survive reboot
        //
        // $SPIW num
        // writes num into DAC - manual osc tunning ? (probably needs manual holdover)
				//
        // $FxRxxH
				// if state == 0 set state = 4
				// else if state == 4 set state = 20.
				// Toggle forced holdover
				//
        // $FxRxxU
				// get leap second from config saved in flash, when no leap second from GPS is available
				//
				// $JAM
				// Jam sync - whatever that means
        //
        // -------- messages to communicate with GPS receiver:
        //
        // $EBG *
        // copy all messages from GPS port to communication port (0 to turn off)
        //
        // $Fxxx
        // sends nmea message to GPS
        // xxx needs to start with $ (eg: $F$PFEC....)
        //
        // $ELOAD - gps firmware loading ... dont bother
        // $AGP - agps load
        //
        // ---- other debug commands:
        // $EBG x - engine debug 0 = disable *=all?
        // $XBG x - console clone (whatever that is)
        // $RBG x - raw debug
        // $CBG x - command debug
        // last 3 debugs are only for jtag uart, they dont change output for comm port
        //
        // unknown commands:
				// $TRAIM x - disables traim evaluation in gpsdo? 0 - skips some computation internally

        // Response Messages:
        // $UPDATE FLASH
				// parameters in flash have been updated.
				//
        // $STATUS 
        // 1: (Maybe 10 MHz bad, based on packrat docs)
        // 2: (Maybe PPS bad, based on packrat docs)
        // 3: Antenna is bad? 0=good
        // 4: Holdover duration (secs)
        // 5: Number of sats tracked (different than, but within 2 of $EXTSTATUS, perhaps only counts channels 0-7???, range is 0-8)
        // Status [Locked = 0, Recovery = 1, (Forced holdover?)=3, Train OXCO=7, Holdover = 8,
		//        [Startup A/B/C/D = 10/11/2/19 ]
        //        [ (transition from 1 to 0) = (14,15,16,17,18) ] Wait states when transitioning
        //        [ (transition from 0 to 1) = (20,21,22) ]  Wait states when transitioning
        //  (6 = locked, but unknown location????)
        //
        // $PPSDBG 1187153266 3 25.28081e3 -253 -6 2 2 0.0
        // $PPSDBG 2 0.0 [Fewer parmaters when in holdover?]
        // 1: same as clock (GPS Time)
        // 2: Same as $STATUS status, but updates much more often (and seems to skip states less often)
        // 3: Floating point number. DAC set value. Tends towards 29e3 on my board. Proportional to the DAC voltage
        //         On my RevC CTS board, Vbias ~= 6.25e-5*PPS3. This may make sense for a 4.096 V reference: 4.096/2^16=6.25e-5
        //         During startup, it is not put in the result string (this field is blank, so two sequential space characters are in the string)
        // 4: Measured phase offset? Units seem something like 6.5*ns
        // 5: PPS offset from $PFEC,GPrrm message - range from -15 to +14
        // 6: PPS status from $PFEC,GPrrm message
        // 7: TRAIM status from $PFEC,GPrrm message 
        // 8: Always 0.0 - temperature on 12.1.1 firmware
        // 
        // $EXTSTATUS
        // 1: SurveyStatus [0=normal, 1=surveying]
        // 2: Number of sats used for positioning, copied from $GPGGA message 
        // 3: HDOP if 2Dfix, PDOP if 3D fix, copied from $GPGGA message
        // 4: Temperature (close to FPGA? close to oven?) (my board reads about 45C)
				// 5: gps discard counter - error related ?
        //
        // $GETPOS (sent after setting position, or requesting position
        // Latitude
        // Longitude
        // Elevation_MSL
        // Correction to MSL to get WGS elevation (add this value to MSL to get WGS ellipsoid)
        // Traim status from GPS. From Furuno GPrrm msg (0 = possible to detect and remove abnormalities, 1 = possible to detect, 2 = not possible to detect) 
        //
        // $SURVEY 40448488 -86915296 225 -34 7129
        // [sent during a survey]
        // 1: Latitude
        // 2: Longitude
        // 3: Elevation_MSL
        // 4: Correction to MSL to get WGS elevation (add this value to MSL to get WGS ellipsoid)
        // 5: Number of seconds remaining
        //
        // $SAT
        // Channel # (0-based, seems to only return channel 0 through 7)
        // GPS Sattelite number
        // Elevation (degree)
        // Azimuth (degree)
        // SNR (dB*Hz)
        //
        // $CLOCK 1187156731 18 3
        // GPS UNIX-timestamp (secs since 1970), but my board is off by 10 years (reporting 2007 while it is 2017)
        // Count of leap-seconds
        // Time figure-of-merit (1=good, 7=bad)
        //
        // $GETVER 12.0.1 BOOT 10 fbde 7437 06162200B0000A2004183ACC
        // $GETVER 12.0.1 0.4850266.0.1 19 fbde 7437 06162200B0000A2004183ACC
        // [version information, I used this to know if I need to send the PROCEED command, String contains BOOT when in bootloader mode]
        // [During boot, only terminated with LF but not CR(or maybe other-way around?)]
        // 1: Bootloader version?
        // 2: ("BOOT" during boot) or (software or perhaps GPS version info????)
        // 3: Status code (same as $STATUS)
        // 4: CRC of something?
        // 5: Always 7437?
        // 6: (Part number: 06162200)(Board Rev: B)(Assembly version: 0000)(Delimeter: A)(Year: 2004)(Week?: 18)(SN?: 3ACC)
        //
        // $WSAT 4 138 209 38 0
        // [WAAS Satellite info, same format as $SAT]
        // 
        // $SET1PPS
        // [Sent at boot, but also in response to a $SET1PPS command. Sent every 20 seconds.]
        //
		// $SETGAIN 1.00
		// [Sent is response to $SETGAIN <n> command]
        // 1: Value of gain. This should be n/100, and is a float.
        // 
        // $KALDBG 1187203779 0.08 29.59241e3 0.120e-3 0.568 0 0
        // [Only be sent when reference is locked (state=0)]
        // 1: GPS UNIX-timestamp (secs since 1970), but my board is off by 10 years (reporting 2007 while it is 2017)
        // 2: Floating point number.
        //     Resets to 0 at time of lock (and at end of holdover).
        // 3: Floating point number. Magnitude is similar to PPS3, but does not track it so well.
        // 4: Floating point.
        //     Resets to 0 at time of lock (and at end of holdover).
        // 5: Floating point. Smoothed version of PPS3, seems like ~6.5*(PPS phase in ns)?
        //     Resets to 0 at time of lock (and at end of holdover).
        // 6: always zero
        // 7: always zero
