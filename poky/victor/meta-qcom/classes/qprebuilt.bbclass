# qprebuilt.bbclass enables use of existing prebuilt binaries, headers and libraries
# when the source code to build a package is not available by bypassing the usual
# fetch/unpack/patch/compile process.
#
# To use this class, add qprebuilt to the global inherit and set SRC_DIR to point at
# the directory which usually contains the sources. If sources are available in SRC_DIR
# regular process is followed. If not prebuilts are used to generate final image.
#

SRC_DIR_TASKS += "do_install do_compile do_patch do_unpack do_fetch"

# Copy artifacts to ${TMPDIR}/prebuilt/${MACHINE}/${PN}
do_prebuilt[dirs] = "${TMPDIR}/prebuilt/${MACHINE}/${PN}"
do_prebuilt() {
    cp -fpPRa ${D}/* ${TMPDIR}/prebuilt/${MACHINE}/${PN}
    cd ${TMPDIR}/prebuilt/${MACHINE}/${PN}
    if [ -f ${PN}-binaries.tar ]; then
        rm -fr ${PN}-binaries.tar
    fi
    tar -cjvf ${PN}-binaries.tar *
}

# Copy from prebuilt_HYxx/machine/PN to image.
# Release contains one of HY11/HY22. Whenever present, prebuilts in HY11 gets priority.
# If prebuilts are not present in HY11/HY22 check in FEAT-BIN directories.
do_prebuilt_install() {
    prebuiltdir="${WORKSPACE}/prebuilt_HY22"
    if [ -d ${WORKSPACE}/prebuilt_HY11 ]; then
        prebuiltdir="${WORKSPACE}/prebuilt_HY11"
    fi

    if [ -f ${prebuiltdir}/${MACHINE}/${PN}/${PN}-binaries.tar ]; then
        echo "Installing from ${prebuiltdir}"
        mkdir -p ${D}
        tar --no-same-owner -xjvf ${prebuiltdir}/${MACHINE}/${PN}/${PN}-binaries.tar -C ${D}
    else
        for path in ${WORKSPACE}/prebuilt_FEAT-BIN-*; do
            [ -f ${path}/${MACHINE}/${PN}/${PN}-binaries.tar ] || continue
            echo "Installing from ${path}"
            mkdir -p ${D}
            tar --no-same-owner -xjvf ${path}/${MACHINE}/${PN}/${PN}-binaries.tar -C ${D}
        done
    fi
}
do_prebuilt_install[dirs] = "${TMPDIR}/prebuilt/${MACHINE}"
do_prebuilt_install[cleandirs] = "${D}"
do_prebuilt_install[fakeroot] = "1"

EXPORT_FUNCTIONS do_prebuilt do_prebuilt_install

# Empty images folder after prebuilt_install indicates that the package has no artifacts to add
# to final image. Mark this as empty package so that it can be ignored while creating rootfs.
do_prebuilt_install[postfuncs] += "mark_empty_packages"
python mark_empty_packages () {
    import os

    destdir = d.getVar('D', True)
    if not (os.path.isdir(destdir) and os.listdir(destdir)):
        tmpdir = d.getVar('TMPDIR', True)
        machine = d.getVar('MACHINE', True)
        prebuilt_path = os.path.join(tmpdir, 'prebuilt', machine)
        if not os.path.isdir(prebuilt_path):
            os.makedirs(prebuilt_path)
        file_path = os.path.join(prebuilt_path, 'empty_pkgs')
        with open(file_path, 'a') as f:
            f.write("%s\n" % d.getVar('PN', True))
        bb.debug(1, "NOTE: Package %s is empty" % d.getVar('PN', True))
}

python __anonymous() {
    import os, bb

    srcdir = d.getVar('SRC_DIR', True)
    if srcdir:
        if os.path.isdir(srcdir):
            # SRC_DIR exists. So add task to copy build artifacts to prebuilt dir.
            bb.build.addtask('do_prebuilt', 'do_build', 'do_populate_sysroot', d)

            # Append SRC_DIR to SRC_URI as 'file' type.
            local_srcuri = (d.getVar('SRC_URI', True) or "").split()
            if not local_srcuri:
                workspace = d.getVar("WORKSPACE", True) + "/"
                url = "file://" + srcdir.replace(workspace, "")
                local_srcuri.append(url)
                d.setVar('SRC_URI', ' '.join(local_srcuri))
        else:
            # SRC_DIR doesn't exist. So remove SRC dependent tasks and
            # add a task to install artifacts from prebuilt dir.
            for task in d.getVar("SRC_DIR_TASKS", True).split():
                bb.build.deltask(task, d)
            bb.build.addtask('do_prebuilt_install', 'do_populate_sysroot', 'do_populate_lic', d)

            # ensure packaging starts after sysroot gets populated.
            d.appendVarFlag('do_package', 'depends', " %s:do_populate_sysroot" % d.getVar('PN', True))

            # Prebuilts are already stripped. Skip sysroot_strip.
            d.setVar('INHIBIT_SYSROOT_STRIP', '1')
    else:
        # Warn if SRC_DIR is not set.
        bb.warn("NOTE: %s: inherits qprebuilt but doesn't set SRC_DIR" % d.getVar('FILE', True))
}
