PKG_NAME=switchv2p
PKG_SRC=./p4/${PKG_NAME}.p4
PYTHON_INCLUDES=./py

build:
	${HOME}/tools/p4_build.sh ${PKG_SRC}
model:
	${SDE}/run_tofino_model.sh -p ${PKG_NAME}
switchd:
	${SDE}/run_switchd.sh -p ${PKG_NAME}
test:
	PYTHONPATH=${PYTHON_INCLUDES}:${PYTHONPATH} ${SDE}/run_p4_tests.sh -p ${PKG_NAME} -t ./ptf-tests
