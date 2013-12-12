r"""paraviewweb_protocols is a module that contains a set of ParaViewWeb related
protocols that can be combined together to provide a flexible way to define
very specific web application.
"""

import os, sys, logging, types, inspect, traceback, logging, re
from time import time

# import RPC annotation
from autobahn.wamp import exportRpc

# import paraview modules.
from paraview import simple, servermanager
from paraview.web import helper
from vtk.web import protocols as vtk_protocols

from vtkWebCorePython import vtkWebInteractionEvent

# =============================================================================
#
# Base class for any ParaView based protocol
#
# =============================================================================

class ParaViewWebProtocol(vtk_protocols.vtkWebProtocol):

    def __init__(self):
        self.Application = None

    def mapIdToProxy(self, id):
        """
        Maps global-id for a proxy to the proxy instance. May return None if the
        id is not valid.
        """
        id = int(id)
        if id <= 0:
            return None
        return simple.servermanager._getPyProxy(\
                simple.servermanager.ActiveConnection.Session.GetRemoteObject(id))

    def getView(self, vid):
        """
        Returns the view for a given view ID, if vid is None then return the
        current active view.
        :param vid: The view ID
        :type vid: str
        """
        view = self.mapIdToProxy(vid)
        if not view:
            # Use active view is none provided.
            view = simple.GetActiveView()

        if not view:
            raise Exception("no view provided: " + vid)

        return view

# =============================================================================
#
# Handle Mouse interaction on any type of view
#
# =============================================================================

class ParaViewWebMouseHandler(ParaViewWebProtocol):

    @exportRpc("mouseInteraction")
    def mouseInteraction(self, event):
        """
        RPC Callback for mouse interactions.
        """
        view = self.getView(event['view'])

        buttons = 0
        if event["buttonLeft"]:
            buttons |= vtkWebInteractionEvent.LEFT_BUTTON;
        if event["buttonMiddle"]:
            buttons |= vtkWebInteractionEvent.MIDDLE_BUTTON;
        if event["buttonRight"]:
            buttons |= vtkWebInteractionEvent.RIGHT_BUTTON;

        modifiers = 0
        if event["shiftKey"]:
            modifiers |= vtkWebInteractionEvent.SHIFT_KEY
        if event["ctrlKey"]:
            modifiers |= vtkWebInteractionEvent.CTRL_KEY
        if event["altKey"]:
            modifiers |= vtkWebInteractionEvent.ALT_KEY
        if event["metaKey"]:
            modifiers |= vtkWebInteractionEvent.META_KEY

        pvevent = vtkWebInteractionEvent()
        pvevent.SetButtons(buttons)
        pvevent.SetModifiers(modifiers)
        pvevent.SetX(event["x"])
        pvevent.SetY(event["y"])
        #pvevent.SetKeyCode(event["charCode"])
        retVal = self.getApplication().HandleInteractionEvent(view.SMProxy, pvevent)
        del pvevent
        return retVal

# =============================================================================
#
# Basic 3D Viewport API (Camera + Orientation + CenterOfRotation
#
# =============================================================================

class ParaViewWebViewPort(ParaViewWebProtocol):

    @exportRpc("resetCamera")
    def resetCamera(self, view):
        """
        RPC callback to reset camera.
        """
        view = self.getView(view)
        simple.ResetCamera(view)
        try:
            view.CenterOfRotation = view.CameraFocalPoint
        except:
            pass

        self.getApplication().InvalidateCache(view.SMProxy)
        return view.GetGlobalIDAsString()

    @exportRpc("updateOrientationAxesVisibility")
    def updateOrientationAxesVisibility(self, view, showAxis):
        """
        RPC callback to show/hide OrientationAxis.
        """
        view = self.getView(view)
        view.OrientationAxesVisibility = (showAxis if 1 else 0);

        self.getApplication().InvalidateCache(view.SMProxy)
        return view.GetGlobalIDAsString()

    @exportRpc("updateCenterAxesVisibility")
    def updateCenterAxesVisibility(self, view, showAxis):
        """
        RPC callback to show/hide CenterAxesVisibility.
        """
        view = self.getView(view)
        view.CenterAxesVisibility = (showAxis if 1 else 0);

        self.getApplication().InvalidateCache(view.SMProxy)
        return view.GetGlobalIDAsString()

    @exportRpc("updateCamera")
    def updateCamera(self, view_id, focal_point, view_up, position):
        view = self.getView(view_id)

        view.CameraFocalPoint = focal_point
        view.CameraViewUp = view_up
        view.CameraPosition = position
        self.getApplication().InvalidateCache(view.SMProxy)

# =============================================================================
#
# Provide Image delivery mechanism
#
# =============================================================================

class ParaViewWebViewPortImageDelivery(ParaViewWebProtocol):

    @exportRpc("stillRender")
    def stillRender(self, options):
        """
        RPC Callback to render a view and obtain the rendered image.
        """
        beginTime = int(round(time() * 1000))
        view = self.getView(options["view"])
        size = [view.ViewSize[0], view.ViewSize[1]]
        if options and options.has_key("size"):
            size = options["size"]
            view.ViewSize = size
        t = 0
        if options and options.has_key("mtime"):
            t = options["mtime"]
        quality = 100
        if options and options.has_key("quality"):
            quality = options["quality"]
        localTime = 0
        if options and options.has_key("localTime"):
            localTime = options["localTime"]
        reply = {}
        app = self.getApplication()
        reply["image"] = app.StillRenderToString(view.SMProxy, t, quality)
        reply["stale"] = app.GetHasImagesBeingProcessed(view.SMProxy)
        reply["mtime"] = app.GetLastStillRenderToStringMTime()
        reply["size"] = [view.ViewSize[0], view.ViewSize[1]]
        reply["format"] = "jpeg;base64"
        reply["global_id"] = view.GetGlobalIDAsString()
        reply["localTime"] = localTime

        endTime = int(round(time() * 1000))
        reply["workTime"] = (endTime - beginTime)

        return reply


# =============================================================================
#
# Provide Geometry delivery mechanism (WebGL)
#
# =============================================================================

class ParaViewWebViewPortGeometryDelivery(ParaViewWebProtocol):

    @exportRpc("getSceneMetaData")
    def getSceneMetaData(self, view_id):
        view  = self.getView(view_id);
        data = self.getApplication().GetWebGLSceneMetaData(view.SMProxy)
        return data

    @exportRpc("getWebGLData")
    def getWebGLData(self, view_id, object_id, part):
        view  = self.getView(view_id)
        data = self.getApplication().GetWebGLBinaryData(view.SMProxy, str(object_id), part-1)
        return data

# =============================================================================
#
# Time management
#
# =============================================================================

class ParaViewWebTimeHandler(ParaViewWebProtocol):

    def __init__(self):
        super(ParaViewWebTimeHandler, self).__init__()
        # setup animation scene
        self.scene = simple.GetAnimationScene()
        simple.GetTimeTrack()
        self.scene.PlayMode = "Snap To TimeSteps"

    @exportRpc("updateTime")
    def updateTime(self,action):
        view = simple.GetRenderView()
        animationScene = simple.GetAnimationScene()
        currentTime = view.ViewTime

        if action == "next":
            animationScene.GoToNext()
            if currentTime == view.ViewTime:
                animationScene.GoToFirst()
        if action == "prev":
            animationScene.GoToPrevious()
            if currentTime == view.ViewTime:
                animationScene.GoToLast()
        if action == "first":
            animationScene.GoToFirst()
        if action == "last":
            animationScene.GoToLast()

        return view.ViewTime

# =============================================================================
#
# Pipeline manager
#
# =============================================================================

class ParaViewWebPipelineManager(ParaViewWebProtocol):

    def __init__(self, baseDir=None, fileToLoad=None):
        super(ParaViewWebPipelineManager, self).__init__()
        # Setup global variables
        self.pipeline = helper.Pipeline('Kitware')
        self.lutManager = helper.LookupTableManager()
        self.view = simple.GetRenderView()
        self.baseDir = baseDir;
        simple.SetActiveView(self.view)
        simple.Render()
        self.view.ViewSize = [800,800]
        self.lutManager.setView(self.view)
        if fileToLoad and fileToLoad[-5:] != '.pvsm':
            try:
                self.openFile(fileToLoad)
            except:
                print "error loading..."

    @exportRpc("reloadPipeline")
    def reloadPipeline(self):
        self.pipeline.clear()
        pxm = simple.servermanager.ProxyManager()

        # Fill tree structure (order-less)
        for proxyInfo in pxm.GetProxiesInGroup("sources"):
            id = str(proxyInfo[1])
            proxy = helper.idToProxy(id)
            parentId = helper.getParentProxyId(proxy)
            self.pipeline.addNode(parentId, id)

        return self.pipeline.getRootNode(self.lutManager)

    @exportRpc("getPipeline")
    def getPipeline(self):
        if self.pipeline.isEmpty():
            return self.reloadPipeline()
        return self.pipeline.getRootNode(self.lutManager)

    @exportRpc("addSource")
    def addSource(self, algo_name, parent):
        pid = str(parent)
        parentProxy = helper.idToProxy(parent)
        if parentProxy:
            simple.SetActiveSource(parentProxy)
        else:
            pid = '0'

        # Create new source/filter
        cmdLine = 'simple.' + algo_name + '()'
        newProxy = eval(cmdLine)

        # Create its representation and render
        simple.Show()
        simple.Render()
        simple.ResetCamera()

        # Add node to pipeline
        self.pipeline.addNode(pid, newProxy.GetGlobalIDAsString())

        # Create LUT if need be
        if pid == '0':
            self.lutManager.registerFieldData(newProxy.GetPointDataInformation())
            self.lutManager.registerFieldData(newProxy.GetCellDataInformation())

        # Return the newly created proxy pipeline node
        return helper.getProxyAsPipelineNode(newProxy.GetGlobalIDAsString(), self.lutManager)

    @exportRpc("deleteSource")
    def deleteSource(self, proxy_id):
        self.pipeline.removeNode(proxy_id)
        proxy = helper.idToProxy(proxy_id)
        simple.Delete(proxy)
        simple.Render()

    @exportRpc("updateDisplayProperty")
    def updateDisplayProperty(self, options):
        proxy = helper.idToProxy(options['proxy_id']);
        rep = simple.GetDisplayProperties(proxy)
        helper.updateProxyProperties(rep, options)

        # Try to bind the proper lookup table if need be
        if options.has_key('ColorArrayName') and len(options['ColorArrayName']) > 0:
            name = options['ColorArrayName']
            type = options['ColorAttributeType']
            nbComp = 1

            if type == 'POINT_DATA':
                data = rep.GetRepresentedDataInformation().GetPointDataInformation()
                for i in range(data.GetNumberOfArrays()):
                    array = data.GetArrayInformation(i)
                    if array.GetName() == name:
                        nbComp = array.GetNumberOfComponents()
            elif type == 'CELL_DATA':
                data = rep.GetRepresentedDataInformation().GetCellDataInformation()
                for i in range(data.GetNumberOfArrays()):
                    array = data.GetArrayInformation(i)
                    if array.GetName() == name:
                        nbComp = array.GetNumberOfComponents()
            lut = self.lutManager.getLookupTable(name, nbComp)
            rep.LookupTable = lut

        simple.Render()

    @exportRpc("pushState")
    def pushState(self, state):
        for proxy_id in state:
            if proxy_id == 'proxy':
                continue
            proxy = helper.idToProxy(proxy_id);
            helper.updateProxyProperties(proxy, state[proxy_id])
            simple.Render()
        return helper.getProxyAsPipelineNode(state['proxy'], self.lutManager)

    @exportRpc("openFile")
    def openFile(self, path):
        reader = simple.OpenDataFile(path)
        simple.RenameSource( path.split("/")[-1], reader)
        simple.Show()
        simple.Render()
        simple.ResetCamera()

        # Add node to pipeline
        self.pipeline.addNode('0', reader.GetGlobalIDAsString())

        # Create LUT if need be
        self.lutManager.registerFieldData(reader.GetPointDataInformation())
        self.lutManager.registerFieldData(reader.GetCellDataInformation())

        return helper.getProxyAsPipelineNode(reader.GetGlobalIDAsString(), self.lutManager)

    @exportRpc("openRelativeFile")
    def openRelativeFile(self, relativePath):
        fileToLoad = []
        if type(relativePath) == list:
            for file in relativePath:
               fileToLoad.append(os.path.join(self.baseDir, file))
        else:
            fileToLoad.append(os.path.join(self.baseDir, files))

        reader = simple.OpenDataFile(fileToLoad)
        name = fileToLoad[0].split("/")[-1]
        if len(name) > 15:
            name = name[:15] + '*'
        simple.RenameSource(name, reader)
        simple.Show()
        simple.Render()
        simple.ResetCamera()

        # Add node to pipeline
        self.pipeline.addNode('0', reader.GetGlobalIDAsString())

        # Create LUT if need be
        self.lutManager.registerFieldData(reader.GetPointDataInformation())
        self.lutManager.registerFieldData(reader.GetCellDataInformation())

        return helper.getProxyAsPipelineNode(reader.GetGlobalIDAsString(), self.lutManager)

    @exportRpc("updateScalarbarVisibility")
    def updateScalarbarVisibility(self, options):
        # Remove unicode
        if options:
            for lut in options.values():
                if type(lut['name']) == unicode:
                    lut['name'] = str(lut['name'])
                self.lutManager.enableScalarBar(lut['name'], lut['size'], lut['enabled'])
        return self.lutManager.getScalarbarVisibility()

    @exportRpc("updateScalarRange")
    def updateScalarRange(self, proxyId):
        proxy = self.mapIdToProxy(proxyId);
        self.lutManager.registerFieldData(proxy.GetPointDataInformation())
        self.lutManager.registerFieldData(proxy.GetCellDataInformation())

    @exportRpc("listFilters")
    def listFilters(self):
        filterSet = [{
                    'name': 'Cone',
                    'icon': 'dataset',
                    'category': 'source'
                },{
                    'name': 'Sphere',
                    'icon': 'dataset',
                    'category': 'source'
                },{
                    'name': 'Wavelet',
                    'icon': 'dataset',
                    'category': 'source'
                },{
                    'name': 'Clip',
                    'icon': 'clip',
                    'category': 'filter'
                },{
                    'name': 'Slice',
                    'icon': 'slice',
                    'category': 'filter'
                },{
                    'name': 'Contour',
                    'icon': 'contour',
                    'category': 'filter'
                },{
                    'name': 'Threshold',
                    'icon': 'threshold',
                    'category': 'filter'
                },{
                    'name': 'StreamTracer',
                    'icon': 'stream',
                    'category': 'filter'
                },{
                    'name': 'WarpByScalar',
                    'icon': 'filter',
                    'category': 'filter'
                }]
        if servermanager.ActiveConnection.GetNumberOfDataPartitions() > 1:
            filterSet.append({ 'name': 'D3', 'icon': 'filter', 'category': 'filter' })
        return filterSet

# =============================================================================
#
# Remote file list
#
# =============================================================================

class ParaViewWebFileManager(ParaViewWebProtocol):

    def __init__(self, defaultDirectoryToList):
        super(ParaViewWebFileManager, self).__init__()
        self.directory = defaultDirectoryToList
        self.dirCache = None

    @exportRpc("listFiles")
    def listFiles(self):
        if not self.dirCache:
            self.dirCache = helper.listFiles(self.directory)
        return self.dirCache


# =============================================================================
#
# Handle remote Connection
#
# =============================================================================

class ParaViewWebRemoteConnection(ParaViewWebProtocol):

    @exportRpc("connect")
    def connect(self, options):
        """
        Creates a connection to a remote pvserver.
        Expect an option argument which should override any of
        those default properties::

            {
            'host': 'localhost',
            'port': 11111,
            'rs_host': None,
            'rs_port': 11111
            }

        """
        ds_host = "localhost"
        ds_port = 11111
        rs_host = None
        rs_port = 11111


        if options:
            if options.has_key("host"):
                ds_host = options["host"]
            if options.has_key("port"):
                ds_port = options["port"]
            if options.has_key("rs_host"):
                rs_host = options["rs_host"]
            if options.has_key("rs_port"):
                rs_host = options["rs_port"]

        simple.Connect(ds_host, ds_port, rs_host, rs_port)

    @exportRpc("reverseConnect")
    def reverseConnect(self, port=11111):
        """
        Create a reverse connection to a server.  Listens on port and waits for
        an incoming connection from the server.
        """
        simple.ReverseConnect(port)

    @exportRpc("pvDisconnect")
    def pvDisconnect(self, message):
        """Free the current active session"""
        simple.Disconnect()


# =============================================================================
#
# Handle remote Connection at startup
#
# =============================================================================

class ParaViewWebStartupRemoteConnection(ParaViewWebProtocol):

    connected = False

    def __init__(self, dsHost = None, dsPort = 11111, rsHost=None, rsPort=22222):
        super(ParaViewWebStartupRemoteConnection, self).__init__()
        if not ParaViewWebStartupRemoteConnection.connected and dsHost:
            ParaViewWebStartupRemoteConnection.connected = True
            simple.Connect(dsHost, dsPort, rsHost, rsPort)

# =============================================================================
#
# Handle State Loading
#
# =============================================================================

class ParaViewWebStateLoader(ParaViewWebProtocol):

    def __init__(self, state_path = None):
        super(ParaViewWebStateLoader, self).__init__()
        if state_path and state_path[-5:] == '.pvsm':
            self.loadState(state_path)

    @exportRpc("loadState")
    def loadState(self, state_file):
        """
        Load a state file and return the list of view ids
        """
        simple.LoadState(state_file)
        ids = []
        for view in simple.GetRenderViews():
            ids.append(view.GetGlobalIDAsString())
        return ids

# =============================================================================
#
# Handle Server File Listing
#
# =============================================================================

class ParaViewWebFileListing(ParaViewWebProtocol):

    def __init__(self, basePath, name, excludeRegex=r"^\.|~$|^\$", groupRegex=r"[0-9]+\."):
        """
        Configure the way the WebFile browser will expose the server content.
         - basePath: specify the base directory that we should start with
         - name: Name of that base directory that will show up on the web
         - excludeRegex: Regular expression of what should be excluded from the list of files/directories
        """
        self.baseDirectory = basePath
        self.rootName = name
        self.pattern = re.compile(excludeRegex)
        self.gPattern = re.compile(groupRegex)
        pxm = simple.servermanager.ProxyManager()
        self.directory_proxy = pxm.NewProxy('misc', 'ListDirectory')
        self.fileList = simple.servermanager.VectorProperty(self.directory_proxy,self.directory_proxy.GetProperty('FileList'))
        self.directoryList = simple.servermanager.VectorProperty(self.directory_proxy,self.directory_proxy.GetProperty('DirectoryList'))

    @exportRpc("listServerDirectory")
    def listServerDirectory(self, relativeDir='.'):
        """
        RPC Callback to list a server directory relative to the basePath
        provided at start-up.
        """
        path = [ self.rootName ]
        if len(relativeDir) > len(self.rootName):
            relativeDir = relativeDir[len(self.rootName)+1:]
            path += relativeDir.replace('\\','/').split('/')

        currentPath = os.path.join(self.baseDirectory, relativeDir)

        self.directory_proxy.List(currentPath)

        # build file/dir lists
        files = []
        if len(self.fileList) > 1:
            for f in self.fileList.GetData():
                if not re.search(self.pattern, f):
                    files.append( { 'label': f })
        elif len(self.fileList) == 1 and not re.search(self.pattern, self.fileList.GetData()):
            files.append( { 'label': self.fileList.GetData() })

        dirs = []
        if len(self.directoryList) > 1:
            for d in self.directoryList.GetData():
                if not re.search(self.pattern, d):
                    dirs.append(d)
        elif len(self.directoryList) == 1 and not re.search(self.pattern, self.directoryList.GetData()):
            dirs.append(self.directoryList.GetData())

        result =  { 'label': relativeDir, 'files': files, 'dirs': dirs, 'groups': [], 'path': path }
        if relativeDir == '.':
            result['label'] = self.rootName

        # Filter files to create groups
        files.sort()
        groups = result['groups']
        groupIdx = {}
        filesToRemove = []
        for file in files:
            fileSplit = re.split(self.gPattern, file['label'])
            if len(fileSplit) == 2:
                filesToRemove.append(file)
                gName = '*.'.join(fileSplit)
                if groupIdx.has_key(gName):
                    groupIdx[gName]['files'].append(file['label'])
                else:
                    groupIdx[gName] = { 'files' : [file['label']], 'label': gName }
                    groups.append(groupIdx[gName])
        for file in filesToRemove:
            gName = '*.'.join(re.split(self.gPattern, file['label']))
            if len(groupIdx[gName]['files']) > 1:
                files.remove(file)
            else:
                groups.remove(groupIdx[gName])

        return result

from boto.s3.connection import S3Connection
import boto
import traceback

print "boto"

class ParaViewWebS3Listing(ParaViewWebProtocol):

    def __init__(self, bucketName, excludeRegex=r"^\.|~$|^\$", groupRegex=r"[0-9]+\."):
        """
        Configure the way the WebFile browser will expose the server content.
         - basePath: specify the base directory that we should start with
         - name: Name of that base directory that will show up on the web
         - excludeRegex: Regular expression of what should be excluded from the list of files/directories
        """

        try:

            self.bucketName = bucketName
            self.pattern = re.compile(excludeRegex)
            self.gPattern = re.compile(groupRegex)

            self.conn = S3Connection('AKIAIA4R7RO42A2R7WRA', 'Piy7Ypo2uIUm9Phf5/ixNh+owmjfD8MVEmRPLHdY')
            self.bucket = self.conn.get_bucket('nasanex')
        except:
            print "Exception"


    @exportRpc("listServerDirectory")
    def listServerDirectory(self, dir=''):
        """
        RPC Callback to list a server directory relative to the basePath
        provided at start-up.
        """

        if dir[0] == '/':
            dir = dir[1:]

        print "++++++ listing %s" % dir

        if dir == '.':
            dir = ''

        path = []
        if len(dir) > 0:
            path += dir.replace('\\','/').split('/')

        files = []
        dirs = []

        try:
            ls_prefix = dir
            if dir.startswith(self.bucketName):
                ls_prefix = dir[len(self.bucketName)+1: ]


            if ls_prefix and not ls_prefix.endswith('/'):
                ls_prefix += '/'



            for entry in self.bucket.list(prefix=str(ls_prefix), delimiter='/'):
                print "entry %s %s " % (type(entry), entry.name)

                name = entry.name[len(ls_prefix):]
                if not name:
                    continue

                if isinstance(entry, boto.s3.prefix.Prefix):
                    dirs.append(name.replace('/', ''))
                elif isinstance(entry, boto.s3.key.Key):
                    file = {'label': name.replace('/', ''), 'size': entry.size}
                    files.append(file)
        except:
            print "exceptions"
            print traceback.format_exc()

        import os.path

        dir_name = os.path.basename(dir)

        result =  { 'label': dir_name, 'files': files, 'dirs': dirs, 'groups': [], 'path': path }

        if not dir:
            result['label'] = self.bucketName
            result['path'] = [self.bucketName]

        print "result: %s" % str(result)

        # Filter files to create groups
        files.sort()
        groups = result['groups']
        groupIdx = {}
        filesToRemove = []
        for file in files:
            fileSplit = re.split(self.gPattern, file['label'])
            if len(fileSplit) == 2:
                filesToRemove.append(file)
                gName = '*.'.join(fileSplit)
                if groupIdx.has_key(gName):
                    groupIdx[gName]['files'].append(file['label'])
                else:
                    groupIdx[gName] = { 'files' : [file['label']], 'label': gName }
                    groups.append(groupIdx[gName])
        for file in filesToRemove:
            gName = '*.'.join(re.split(self.gPattern, file['label']))
            if len(groupIdx[gName]['files']) > 1:
                files.remove(file)
            else:
                groups.remove(groupIdx[gName])

        return result