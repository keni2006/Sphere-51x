# SphereServer-0.51x
Potential Update to the Recovered 0.51a aka (0.52) Source given by Westy before he passed<BR>
https://github.com/Sphereserver/Source-Archive/tree/main/0.52

*TODO fix some problems with protocol,fix server crashes after using inccorect values and parametres in scripts
*TODO make pvm and pvp points charge correctly by zones 
*TODO make mysql data store without SAVEWORLD

<b>18.10.2024<BR>
-Should be tested. Not working correctly now *TODO should be save in sphereworld like PVPPOINTS=VALUE
New Feature: PvP Points 
src.pvpPoints 


SPHERESERVER 0.51x  WISHLIST  (3/17/21)
-----------------------------

<b>[ISSUE 02]</b><BR>
-sphere to run in the notification area next to the clock!!!

![alt text](http://dragonsoftime.com/junk/taskbar.png)





<b>[ISSUE 05]</b><BR>
-COLORED ITEM NAMES
	
![alt text](http://dragonsoftime.com/junk/coloreditems.png)	



<b>[ISSUE 07]</b><BR>
-walk and shoot would be nice, shoot and run,<BR>
but half accuracy and less range when moving?   <B>50% less accurate and only 8 range  ???</b>


<b>[ISSUE 11]</b><BR>
-no shooting arrows through multis floors!
example... put a npc on the roof of a tower, you can shoot him from the 1st floor,<BR>
<b>maybe check LOS for Z ?</b>

	
<b>[ISSUE 12]</b><BR>
-Provocation - you can use provoc on player in guarded area, then call guards on them after it makes you attack them
check area for guards?  maybe cant be used on other players?  not really sure how provoc is supposed to work,
but i know i disabled it 18+ years ago for a reason, maybe provoc cant target a player?



<b>[ISSUE 14]</b><BR>
-see whos online?  maybe a way to see whos online?  .online command
maybe just use the hardcoded .admin menu and remove account information, maybe just a simple list with only characters names


<b>[ISSUE 15]</b><BR>
-the greatest thing about the new sphere is [FUNCTION  f_xxx]
you can make your own functions and solve just about any problem with a function
and TAG.YOURMOM=1 same as VAR. i think, but permanent, great way to tag characters and items for scripting


<b>[ISSUE 16]</b><BR>
-SECURITY!!! im sure all those antique injection hacks still work


<b>[ISSUE 17]</b><BR>
-tighten up loose ends, make sure all the skills do what they are supposed to =)


<b>[ISSUE 18]</b><BR>
-tillerman, arent you supposed to pin a map and drop it on the tiller man and hes supposed to go there?


<b>[ISSUE 20]</b><BR>
-explosion potions, be able to set the low and high damage. you can raise the more 2, but the range is so big


<b>[ISSUE 21]</b><BR>
NEW TRIGGERS !!!???
	
--characters--

a trigger so NPCS will look at eachother 

ON=@SeeNewNPC ??  a way to get npcs to fight (besides berserker brain)


<b>[ISSUE 24]</b><BR>
-magic resistance... no matter your skill, you take full spell damage, reistance doesnt work
https://web.archive.org/web/20000306034831fw_/http://uo.stratics.com/resistance.htm


<b>[ISSUE 25]</b><BR>
-multi and/or custom maps?


<b>[ISSUE 26]</b><BR>
-animal taming... <BR>even with 100.0 skill animals with high INT are almost impossible to tame (nightmares ect...)<BR>
<B>maybe decrease INT check by 50% ??? </b>


<b>[ISSUE 27]</b><BR>
-spell interupt? in 51a there is no spell interupt,<BR>
maybe added a SMALL chance (25% or less) of fizzling a spell when taking damage while casting


<b>[ISSUE 29]</b><BR>
-weapon speed in sphereitem.scp never worked<BR>
SPEED=<BR>
value no difference, BUT you could increase weight to adjust swing speed!


<b>[ISSUE 30]</b><BR>
NPC spell targets<BR>
Add healing to self if a monster casts healing<BR>
Monsters should be able to use greater heal, dispel, magic reflection, cure...<BR>

<P><BR><P><BR>
  
  SUMMARY - basically Lil Sphere by Fallout added some nice features to 0.51a, id like to add ALL those features
  and add a few new things and tidy up the 0.51a source for those who still have old server files...
  for those who dont know...<BR> there was a re-write 0.53 that changed the structure for what we know as the modern versions of Sphere, that
  0.53 is what lead up to 55i version alot of people used, this was BEFORE that... servers that used TUS, GreyServer, 0.48e. 0.51a... 1998 to 2000
 
BLAST FROM THE PAST 

https://web.archive.org/web/19991013081655/http://menasoft.com:80/

https://web.archive.org/web/20001009023001/http://menace.ne.mediaone.net/spheres1.htm

https://web.archive.org/web/20000706193639/http://menace.ne.mediaone.net/

https://web.archive.org/web/20000819050744/http://www.sphereserver.com/

https://web.archive.org/web/20000621160908/http://sphereserver.dslwatch.com/

<P><BR><P><BR>


FINISHED ITEMS!!!
---------------------------

<b>[ISSUE 01]</b><BR>
-HITPOINTS - on NPCS, there needs to be a seperation between STR and hitpoints, 
i want npc characters with 200STR AND 1000hp.<BR>
by default HITPOINTS will drain until they are equal with STR. so maybe add...
```
MAXHITS=1000
MAXSTAM=1000
MAXMANA=1000
```
and correct the regen???

so if players and npcs are over their maxhits, hp drains to equal their maxhits
and if players and npcs are under their maxhits, they heal to equal their maxhits not STR
	
![alt text](http://dragonsoftime.com/junk/maxstats2.png)

(optional)
AND FOR PLAYERS i want players to have x2 multiplyer for hitpoints, stamina, mana
so 
```
125 STR = 250 hitpoints
125 DEX = 250 stamina
125 INT = 250 mana
```
(this is option in sphere.ini of course!)

<b>[ISSUE 03]</b>
-COLORED SYSMESSAGE, that lil sphere im using has it... looks like this
```
SRC.SYSMESSAGE #0033,3,You have gained a reward!
```
![alt text](http://dragonsoftime.com/junk/coloredsysmessage.png)

(like Lil Sphere)<BR>
<b>[ DONE! ]</b>

-COLORED SPEECH?! 
```
SRC.SAY #0033,3,I talk in color!
```
<b>[ DONE! ]</b>

<b>[ISSUE 04]</b><BR>
-COLORED NAMES on NPCs

![alt text](http://dragonsoftime.com/junk/colorednames.png)

```
TAG.NAME.HUE 021
NAMEHUE=021
```
nice to be able to change the color of NPCs name when single clicked

![alt text](https://i.gyazo.com/c251800168fead568651171a6a2156a7.png)

FIXED particly, later add change via scripts 

<b>[ DONE! ]</b>

<b>[ISSUE 06]</b><BR>
-Show damage above npc when attacked  (like LiL Sphere)<BR>
 
![alt text](http://dragonsoftime.com/junk/showdamage.png)<BR>
(fix to match Lil Sphere)<BR>
<b>[ DONE! ]</b>

<b>[ISSUE 08]</b><BR>
-colored multis??? new sphere you can go debug mode and set the color on a multi and it will change color in game
after 17 years i just realized you can dye a ship deed and the multi will be affected, but only the parts are colored =)

![alt text](http://dragonsoftime.com/junk/coloredmultis.png)

CLASSICUO WILL DISPLAY COLORED MULTIS! ORION AND ORIGINAL CLIENT WILL NOT!<BR>
<b>[ DONE! ]</b>

<b>[ISSUE 09]</b><BR>
-sphere script loading times are abnormally slow compared to the 51a release version<BR>
<b>[ DONE! ]</b>   
	
<b>[ISSUE 10]</b><BR>
-when you run from an npc with a bow, he shoots a LOT of arrow animations,  you get hit multiple times!
one arrow per step, so when a npc runs at you, its like a machine gun<BR>
[DONE?]

<b>[ISSUE 13]</b><BR>
-TRACKING - make sure "tracking players" actually track players only, ect... (i remember something wrong with tracking)<BR>
 [DONE!]

<b>[ISSUE 19]</b><BR>
-books?  the books in SPHEREBook.scp never worked, the pages are empty when you add them in game
MORE=X  is book #
<b>[ DONE! ]</b>

New Triggers
ON=@LOGIN  (like lil Sphere)  [DONE!]

ON=@LOGOUT  (like lil Sphere)  [DONE!]

ON=@GetHit  [DONE?]

ON=@Hit  [DONE?]

ON=@NotoSend
another nice one that will change the color of a moused over character
```
	ON=@NotoSend//2 ALLY 5 ENEMY 6 RED	
	IF (<SRC.TAG0.MILITARY>)
	 IF !<GUILD>
		ARGN1=5
		ELIF <GUILD>
		ARGN1=2
		ENDIF
	ENDIF
```
 <BR><b>[ DONE! ]</b> 

 <b>[ISSUE 21]</b><BR>
NEW TRIGGERS !!!???

--items--

ONTRIGGER=DROPON_ITEM   in source, but not finished

ONTRIGGER=CLICK  (single click) used for colored NAME.HUE!?

ONTRIGGER=ITEMCLICK  (single item click) used for colored item name?  NAME.HUE!?

ONTRIGGER=DROPON_SELF
	heres a nice lil bag script in the new sphere that will let you only drop runes in a bag...
 ```
	ONTRIGGER=DROPON_SELF
	IF !(<ARGO.TYPE>==20)
	SRC.SYSMESSAGE You may only place runes inside this bag.
	RETURN 1
	ENDIF
```

since there is no runebooks in 51a, a newbied bag that will only accept runes would be nice
	
<b>[ISSUE 22]</b><BR>
-Poison - when posion is cast on someone, the timer on the memory item is 120 ticks before the poison effect starts.
thats way too long, also poison needs to be dehardcoded so the different posion levels damages can be adjusted<BR>
<b>[ DONE! ]</b>   


<b>[ISSUE 23]</b><BR>
-yell distance! ! it would be nice to adjust the distance when players yell in game 20 squares, 50 squares, ect...
players can communicate cross map by yelling =P
SET YELLDISTANCE IN SPHERE.INI ???<BR>
<b>[ DONE! ]</b> 

<b>[ISSUE 28]</b><BR>
-CAST_TIME=10 in spheretables.scp never worked, cast time was hardcoded and could not be changed<BR>
value made little or no difference<BR>
-MANAUSE=  add<BR>
<b>[ DONE! ]</b> 

