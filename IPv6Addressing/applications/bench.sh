#!/bin/bash
script_name=$0
rel_path=$(dirname "$0")
$rel_path/bin/test_rmem 10000 500 mem $1
$rel_path/bin/test_rmem 10000 500 disk $1
$rel_path/bin/test_rmem 10000 500 rmem $1
$rel_path/bin/test_rmem 10000 500 rrmem $1
