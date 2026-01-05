===== FEATURES =====
file manip:
[DONE] delete
[DONE] rename
[DONE] move with drag and drop

[KINDA] add Reed-Solomon for redundancy (error checking only) (no erasures)
+++[KINDA] could make a backup too really easily (check for erasures -- use backup instead)
+++[KINDA] could also try lseek in bck in case rs fails
+++[KINDA] #define BACKUP_PATH


===== GUI =====
[DONE] update gui. make it look nice
[DONE] add a button that closes the client on the connect screen
[DONE] create directory opens a popup to ask for name
[DONE] upload file dialogue with sdl3/native-file-dialogue
+++[DONE] allow multiple files being sent
+++[DONE] don't allow dirs


===== FIXES =====
[DONE] text input alphanumeric checking

[DONE] server uses ids for storing files
[NOPE] send encrypted filenames to server

[DONE] client don't overwrite files in cwd --->
[DONE] don't overwrite files in downloads add (1) at end
+++[DONE] add xml_utils getuniquefilename(cwd, filename)


===== UPDATE DOCS =====
[]


===== BONUS =====
[NOPE] split worker.cpp into more files
