# PyQt4 imports
from PyQt4 import QtGui, QtCore, QtOpenGL
from PyQt4.QtOpenGL import QGLWidget
# PyOpenGL imports
import OpenGL.GL as gl
import OpenGL.arrays.vbo as glvbo
# PyOpenCL imports
import pyopencl as cl
from pyopencl.tools import get_gl_sharing_context_properties

# OpenCL kernel that generates a sine function.
clkernel = """
__kernel void clkernel(__global float2* clpos, __global float2* glpos)
{
    //get our index in the array
    unsigned int i = get_global_id(0);

    // copy the x coordinate from the CL buffer to the GL buffer
    glpos[i].x = clpos[i].x;

    // calculate the y coordinate and copy it on the GL buffer
    glpos[i].y = 0.5 * sin(10.0 * clpos[i].x);
}
"""

def clinit():
    """Initialize OpenCL with GL-CL interop.
    """
    plats = cl.get_platforms()
    # handling OSX
    if sys.platform == "darwin":
        ctx = cl.Context(properties=get_gl_sharing_context_properties(),
                             devices=[])
    else:
        ctx = cl.Context(properties=[
                            (cl.context_properties.PLATFORM, plats[0])]
                            + get_gl_sharing_context_properties())
    queue = cl.CommandQueue(ctx)
    return ctx, queue

class GLPlotWidget(QGLWidget):
    # default window size
    width, height = 600, 600

    def set_data(self, data):
        """Load 2D data as a Nx2 Numpy array.
        """
        self.data = data
        self.count = data.shape[0]

    def initialize_buffers(self):
        """Initialize OpenGL and OpenCL buffers and interop objects,
        and compile the OpenCL kernel.
        """
        # empty OpenGL VBO
        self.glbuf = glvbo.VBO(data=np.zeros(self.data.shape),
                               usage=gl.GL_DYNAMIC_DRAW,
                               target=gl.GL_ARRAY_BUFFER)
        self.glbuf.bind()
        # initialize the CL context
        self.ctx, self.queue = clinit()
        # create a pure read-only OpenCL buffer
        self.clbuf = cl.Buffer(self.ctx,
                            cl.mem_flags.READ_ONLY | cl.mem_flags.COPY_HOST_PTR,
                            hostbuf=self.data)
        # create an interop object to access to GL VBO from OpenCL
        self.glclbuf = cl.GLBuffer(self.ctx, cl.mem_flags.READ_WRITE,
                            int(self.glbuf.buffer))
        # build the OpenCL program
        self.program = cl.Program(self.ctx, clkernel).build()
        # release the PyOpenCL queue
        self.queue.finish()

    def execute(self):
        """Execute the OpenCL kernel.
        """
        # get secure access to GL-CL interop objects
        cl.enqueue_acquire_gl_objects(self.queue, [self.glclbuf])
        # arguments to the OpenCL kernel
        kernelargs = (self.clbuf,
                      self.glclbuf)
        # execute the kernel
        self.program.clkernel(self.queue, (self.count,), None, *kernelargs)
        # release access to the GL-CL interop objects
        cl.enqueue_release_gl_objects(self.queue, [self.glclbuf])
        self.queue.finish()

    def update_buffer(self):
        """Update the GL buffer from the CL buffer
        """
        # execute the kernel before rendering
        self.execute()
        gl.glFlush()

    def initializeGL(self):
        """Initialize OpenGL, VBOs, upload data on the GPU, etc.
        """
        # initialize OpenCL first
        self.initialize_buffers()
        # set background color
        gl.glClearColor(0,0,0,0)
        # update the GL buffer from the CL buffer
        self.update_buffer()

    def paintGL(self):
        """Paint the scene.
        """
        # clear the GL scene
        gl.glClear(gl.GL_COLOR_BUFFER_BIT)
        # set yellow color for subsequent drawing rendering calls
        gl.glColor(1,1,0)
        # bind the VBO
        self.glbuf.bind()
        # tell OpenGL that the VBO contains an array of vertices
        gl.glEnableClientState(gl.GL_VERTEX_ARRAY)
        # these vertices contain 2 simple precision coordinates
        gl.glVertexPointer(2, gl.GL_FLOAT, 0, self.glbuf)
        # draw "count" points from the VBO
        gl.glDrawArrays(gl.GL_LINE_STRIP, 0, self.count)

    def resizeGL(self, width, height):
        """Called upon window resizing: reinitialize the viewport.
        """
        # update the window size
        self.width, self.height = width, height
        # paint within the whole window
        gl.glViewport(0, 0, width, height)
        # set orthographic projection (2D only)
        gl.glMatrixMode(gl.GL_PROJECTION)
        gl.glLoadIdentity()
        # the window corner OpenGL coordinates are (-+1, -+1)
        gl.glOrtho(-1, 1, 1, -1, -1, 1)

if __name__ == '__main__':
    import sys
    import numpy as np

    # define a Qt window with an OpenGL widget inside it
    class TestWindow(QtGui.QMainWindow):
        def __init__(self):
            super(TestWindow, self).__init__()
            # generate random data points
            self.data = np.zeros((10000,2))
            self.data[:,0] = np.linspace(-1.,1.,len(self.data))
            self.data = np.array(self.data, dtype=np.float32)
            # initialize the GL widget
            self.widget = GLPlotWidget()
            self.widget.set_data(self.data)
            # put the window at the screen position (100, 100)
            self.setGeometry(100, 100, self.widget.width, self.widget.height)
            self.setCentralWidget(self.widget)
            self.show()

    # create the Qt App and window
    app = QtGui.QApplication(sys.argv)
    window = TestWindow()
    window.show()
    app.exec_()