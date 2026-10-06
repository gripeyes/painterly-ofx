"""Dedicated GUI verification session; does not modify the user's open script."""
import nuke
import os
from PySide6.QtCore import QTimer

def show_artist_node():
    if nuke.exists("Pigment_UI_Validation"):
        return
    # Match the standalone host-comparison scene, not the user's startup OCIO
    # input roles. This changes only this dedicated validation script.
    nuke.root()['colorManagement'].setValue('Nuke')
    kind = next(t for t in nuke.nodeTypes(force_plugin_load=True)
                if t.lower() == "pigment" or "org.painterlyofx.pigment_v1" in t.lower())
    node = nuke.createNode(kind, inpanel=True)
    node.setName("Pigment_UI_Validation")
    read=nuke.nodes.Read(file='/Users/j7s/coding/painterly-ofx/tests/visual/renders/phase4/comparative-pipeline/fashion/source.png')
    assert node['pigmentInterface'].value()=='Pigment'
    assert node['phase4ExecutionClass'].value()=='Interactive / Guided'
    preview=nuke.nodes.Reformat(inputs=[read],type='to box',box_width=1920,box_height=1080,resize='distort')
    node.setInput(0,preview)
    nuke.connectViewer(0,node)
    nuke.activeViewer().node()['viewerProcess'].setValue('sRGB')
    nuke.show(node)
    nuke.scriptSaveAs(os.path.join(os.path.dirname(__file__),'..','build','phase4-ui-evidence','GuidedArtistBaseline.nk'),overwrite=1)
    print("PIGMENT_UI_CREATED", node["pigmentInterface"].value(), flush=True)

QTimer.singleShot(1000, show_artist_node)
