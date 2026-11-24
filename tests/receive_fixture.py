"""Compile the production mesh and UI callbacks against the host test radio and serial port."""

def write_callbacks(repo):
    root = repo / "examples/companion_radio"
    methods = []
    for source, signature in [("MyMesh.cpp", "void MyMesh::onControlDataRecv("),
                              ("MyMesh.cpp", "void MyMesh::cancelPocketRequest("),
                              ("MyMesh.cpp", "void MyMesh::logTx("),
                              ("MyMesh.cpp", "void MyMesh::logTxFail("),
                              ("ui-new/UITask.cpp", "void UITask::onPocketTxDone("),
                              ("ui-new/UITask.cpp", "void UITask::onControlDataRecv(")]:
        text = (root / source).read_text()
        start = text.index(signature)
        # Each production method ends with an unindented closing brace.
        methods.append(text[start:text.index("\n}", start) + 2])
    (repo / "t1-test-callbacks.h").write_text("\n".join(methods))
