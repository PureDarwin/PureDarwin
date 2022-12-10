define(`concat', `translit(``$*'', ` 	')')
define(`fsent', `format(`%-25s %-16s nfs    %-16s 0 0', `$1:$2', `$3', concat$4)')

fsent(freja, /home/gevn, /home/gevn, (rw, soft, bg, grpid))
fsent(freja, /home/freja, /home/freja, (rw, soft, grpid))
fsent(rimfaxe, /home/rimfaxe, /home/rimfaxe, (rw, soft, bg))

