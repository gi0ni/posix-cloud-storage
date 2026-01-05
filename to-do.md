===== FEATURES =====
file manip:
[DONE] delete
[DONE] rename
[] move with drag and drop

[] add Reed-Solomon for redundancy (error checking only) (no erasures)
+++ could make a backup too really easily (check for erasures -- use backup instead)
+++ could also try lseek in bck in case rs fails
+++ #define BACKUP_PATH


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

[] client don't overwrite files in cwd --->
[] don't overwrite files in downloads add (1) at end
+++ add xml_utils getuniquefilename(cwd, filename)


===== UPDATE DOCS =====
[]


===== BONUS =====
[] split worker.cpp into more files
