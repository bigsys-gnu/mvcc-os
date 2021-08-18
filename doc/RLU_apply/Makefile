ROOT_DIR = $(shell pwd)

RCU_FOLDER = rcu
RLU_FOLDER = rlu
RCU_DIR = ${ROOT_DIR}/${RCU_FOLDER}
RLU_DIR = ${ROOT_DIR}/${RLU_FOLDER}

include ${RLU_DIR}/makefile.var
include ${RCU_DIR}/makefile.var

FOLDERS += ${RCU_FOLDER}
FOLDERS += ${RLU_FOLDER}

LOCATOR += local_dir.sty
TARGET += ${LOCATOR}
TARGET += ${RLU_TARGET}
TARGET += ${RLU_TARGET}
OUT_DIC = /tmp

OPTION += -shell-escape 
OPTION += -output-directory=${OUT_DIC}

all: ${RCU_TARGET} ${RLU_TARGET}

${RCU_TARGET}: %: ${RCU_DIR}/%.tex ${LOCATOR}
	pdflatex ${OPTION} $<
	pdflatex ${OPTION} $<

${RLU_TARGET}: %: ${RLU_DIR}/%.tex ${LOCATOR}
	pdflatex ${OPTION} $<
	pdflatex ${OPTION} $<

${LOCATOR}: create_dir_link
	./create_dir_link ${FOLDERS}

clean:
	rm *~ ${addprefix ${OUT_DIC}/,\
*.log\
*.aux\
*.fls\
*.nav\
*.out\
*.snm\
*.toc\
*.vrb}\
 -rf
	rm ${LOCATOR} -f
