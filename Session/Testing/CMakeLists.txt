GVis_add_test(TestReflectionGadgetToVariantMap GVis::Session GVis::Common)
GVis_add_test(TestReflectionObjectSerialize GVis::Session GVis::Common)

target_sources(TestReflectionGadgetToVariantMap PRIVATE
    TestGadgets.h
)

target_sources(TestReflectionObjectSerialize PRIVATE
    TestQObjects.h
)