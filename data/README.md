# SAT_data
This folder contains datasets to test the efficiency of SAT Solver's distribution. You can find standard datasets:
- T10I4D100K.dat & T40I4D100K.dat 
- accidents.dat
- chess.dat
- connect.dat
- kosarak.dat
- mushroom.dat
- pumsb.dat
- retail.dat

A new dataset is also proposed on Tourism visits from Tripadvisor. Data have been scrapped from Tripadvisor from 2016 to 2021 on French locations.
Each transaction corresponds to the list of locations visited by a tourist (same ID). Data have been fully anonymized (both user ID and location ID - no dates).
We have focused on France and a region of France *Hauts-de-France* (HF_)

To understand the effect of aggregation on visiting data, we propose 5 more datasets which corresponds to different geographic level of aggregation (from town to region).

- *HF_tripAdvisorSAT_areaX.csv*
  - CSV file (tab separator "\t")
  - column 1 : user's country (value = 100 000 000 + idCountry) - for constraint programming. Corresponding table "*tripCountries.csv*"
  - next columns:
    - File Area0 : idLocation (visited location - anonymized).
    - File Area5 : **Town** scale.
    - File Area4 : **City**.
    - File Area3 : **District**.
    - File Area2 : **Department**.
    - File Area1 : **Region**.

The "*gadm36.csv*" file gives the correspondance between *column 1* and areas (town, city, district, department, region)
