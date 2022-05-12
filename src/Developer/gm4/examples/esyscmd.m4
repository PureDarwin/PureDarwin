define(`hostname', esyscmd(`hostname'))dnl
`hostname = >>'hostname`<<'
define(`hostname', 
pushdef(`_tmp', `$1')_tmp(translit(esyscmd(`hostname'), `.', `,'))`'popdef(`_tmp'))dnl
`hostname = >>'hostname`<<'
